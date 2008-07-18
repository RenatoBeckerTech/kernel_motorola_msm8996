/*
 * linux/arch/mips/pci/pci-tx4927.c
 *
 * Based on linux/arch/mips/txx9/rbtx4938/setup.c,
 *	    and RBTX49xx patch from CELF patch archive.
 *
 * Copyright 2001, 2003-2005 MontaVista Software Inc.
 * Copyright (C) 2004 by Ralf Baechle (ralf@linux-mips.org)
 * (C) Copyright TOSHIBA CORPORATION 2000-2001, 2004-2007
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <asm/txx9/generic.h>
#include <asm/txx9/tx4927.h>

int __init tx4927_report_pciclk(void)
{
	int pciclk = 0;

	printk(KERN_INFO "PCIC --%s PCICLK:",
	       (__raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_PCI66) ?
	       " PCI66" : "");
	if (__raw_readq(&tx4927_ccfgptr->pcfg) & TX4927_PCFG_PCICLKEN_ALL) {
		u64 ccfg = __raw_readq(&tx4927_ccfgptr->ccfg);
		switch ((unsigned long)ccfg &
			TX4927_CCFG_PCIDIVMODE_MASK) {
		case TX4927_CCFG_PCIDIVMODE_2_5:
			pciclk = txx9_cpu_clock * 2 / 5; break;
		case TX4927_CCFG_PCIDIVMODE_3:
			pciclk = txx9_cpu_clock / 3; break;
		case TX4927_CCFG_PCIDIVMODE_5:
			pciclk = txx9_cpu_clock / 5; break;
		case TX4927_CCFG_PCIDIVMODE_6:
			pciclk = txx9_cpu_clock / 6; break;
		}
		printk("Internal(%u.%uMHz)",
		       (pciclk + 50000) / 1000000,
		       ((pciclk + 50000) / 100000) % 10);
	} else {
		printk("External");
		pciclk = -1;
	}
	printk("\n");
	return pciclk;
}

int __init tx4927_pciclk66_setup(void)
{
	int pciclk;

	/* Assert M66EN */
	tx4927_ccfg_set(TX4927_CCFG_PCI66);
	/* Double PCICLK (if possible) */
	if (__raw_readq(&tx4927_ccfgptr->pcfg) & TX4927_PCFG_PCICLKEN_ALL) {
		unsigned int pcidivmode = 0;
		u64 ccfg = __raw_readq(&tx4927_ccfgptr->ccfg);
		pcidivmode = (unsigned long)ccfg &
			TX4927_CCFG_PCIDIVMODE_MASK;
		switch (pcidivmode) {
		case TX4927_CCFG_PCIDIVMODE_5:
		case TX4927_CCFG_PCIDIVMODE_2_5:
			pcidivmode = TX4927_CCFG_PCIDIVMODE_2_5;
			pciclk = txx9_cpu_clock * 2 / 5;
			break;
		case TX4927_CCFG_PCIDIVMODE_6:
		case TX4927_CCFG_PCIDIVMODE_3:
		default:
			pcidivmode = TX4927_CCFG_PCIDIVMODE_3;
			pciclk = txx9_cpu_clock / 3;
		}
		tx4927_ccfg_change(TX4927_CCFG_PCIDIVMODE_MASK,
				   pcidivmode);
		printk(KERN_DEBUG "PCICLK: ccfg:%08lx\n",
		       (unsigned long)__raw_readq(&tx4927_ccfgptr->ccfg));
	} else
		pciclk = -1;
	return pciclk;
}
