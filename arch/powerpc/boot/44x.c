/*
 * Copyright 2007 David Gibson, IBM Corporation.
 *
 * Based on earlier code:
 *   Matt Porter <mporter@kernel.crashing.org>
 *   Copyright 2002-2005 MontaVista Software Inc.
 *
 *   Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 *   Copyright (c) 2003, 2004 Zultys Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <stddef.h>
#include "types.h"
#include "string.h"
#include "stdio.h"
#include "ops.h"
#include "reg.h"
#include "dcr.h"

/* Read the 44x memory controller to get size of system memory. */
void ibm44x_fixup_memsize(void)
{
	int i;
	unsigned long memsize, bank_config;

	memsize = 0;
	for (i = 0; i < ARRAY_SIZE(sdram_bxcr); i++) {
		mtdcr(DCRN_SDRAM0_CFGADDR, sdram_bxcr[i]);
		bank_config = mfdcr(DCRN_SDRAM0_CFGDATA);

		if (bank_config & SDRAM_CONFIG_BANK_ENABLE)
			memsize += SDRAM_CONFIG_BANK_SIZE(bank_config);
	}

	dt_fixup_memory(0, memsize);
}

#define SPRN_DBCR0		0x134
#define   DBCR0_RST_SYSTEM	0x30000000

void ibm44x_dbcr_reset(void)
{
	unsigned long tmp;

	asm volatile (
		"mfspr	%0,%1\n"
		"oris	%0,%0,%2@h\n"
		"mtspr	%1,%0"
		: "=&r"(tmp) : "i"(SPRN_DBCR0), "i"(DBCR0_RST_SYSTEM)
		);

}
