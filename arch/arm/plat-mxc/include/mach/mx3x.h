/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MXC_MX31_H__
#define __ASM_ARCH_MXC_MX31_H__

/*
 * MX31 memory map:
 *
 * Virt		Phys		Size	What
 * ---------------------------------------------------------------------------
 * FC000000	43F00000	1M	AIPS 1
 * FC100000	50000000	1M	SPBA
 * FC200000	53F00000	1M	AIPS 2
 * FC500000	60000000	128M	ROMPATCH
 * FC400000	68000000	128M	AVIC
 *         	70000000	256M	IPU (MAX M2)
 *         	80000000	256M	CSD0 SDRAM/DDR
 *         	90000000	256M	CSD1 SDRAM/DDR
 *         	A0000000	128M	CS0 Flash
 *         	A8000000	128M	CS1 Flash
 *         	B0000000	32M	CS2
 *         	B2000000	32M	CS3
 * F4000000	B4000000	32M	CS4
 *         	B6000000	32M	CS5
 * FC320000	B8000000	64K	NAND, SDRAM, WEIM, M3IF, EMI controllers
 *         	C0000000	64M	PCMCIA/CF
 */

/*
 * L2CC
 */
#define MX3x_L2CC_BASE_ADDR		0x30000000
#define MX3x_L2CC_SIZE			SZ_1M

/*
 * AIPS 1
 */
#define MX3x_AIPS1_BASE_ADDR		0x43f00000
#define MX3x_AIPS1_BASE_ADDR_VIRT	0xfc000000
#define MX3x_AIPS1_SIZE			SZ_1M
#define MX3x_MAX_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x04000)
#define MX3x_EVTMON_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x08000)
#define MX3x_CLKCTL_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x0c000)
#define MX3x_ETB_SLOT4_BASE_ADDR		(MX3x_AIPS1_BASE_ADDR + 0x10000)
#define MX3x_ETB_SLOT5_BASE_ADDR		(MX3x_AIPS1_BASE_ADDR + 0x14000)
#define MX3x_ECT_CTIO_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x18000)
#define MX3x_I2C_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x80000)
#define MX3x_I2C3_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x84000)
#define MX3x_UART1_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x90000)
#define MX3x_UART2_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x94000)
#define MX3x_I2C2_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x98000)
#define MX3x_OWIRE_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0x9c000)
#define MX3x_SSI1_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xa0000)
#define MX3x_CSPI1_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xa4000)
#define MX3x_KPP_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xa8000)
#define MX3x_IOMUXC_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xac000)
#define MX3x_ECT_IP1_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xb8000)
#define MX3x_ECT_IP2_BASE_ADDR			(MX3x_AIPS1_BASE_ADDR + 0xbc000)

/*
 * SPBA global module enabled #0
 */
#define MX3x_SPBA0_BASE_ADDR		0x50000000
#define MX3x_SPBA0_BASE_ADDR_VIRT	0xfc100000
#define MX3x_SPBA0_SIZE			SZ_1M
#define MX3x_UART3_BASE_ADDR			(MX3x_SPBA0_BASE_ADDR + 0x0c000)
#define MX3x_CSPI2_BASE_ADDR			(MX3x_SPBA0_BASE_ADDR + 0x10000)
#define MX3x_SSI2_BASE_ADDR			(MX3x_SPBA0_BASE_ADDR + 0x14000)
#define MX3x_ATA_DMA_BASE_ADDR			(MX3x_SPBA0_BASE_ADDR + 0x20000)
#define MX3x_MSHC1_BASE_ADDR			(MX3x_SPBA0_BASE_ADDR + 0x24000)
#define MX3x_SPBA_CTRL_BASE_ADDR		(MX3x_SPBA0_BASE_ADDR + 0x3c000)

/*
 * AIPS 2
 */
#define MX3x_AIPS2_BASE_ADDR		0x53f00000
#define MX3x_AIPS2_BASE_ADDR_VIRT	0xfc200000
#define MX3x_AIPS2_SIZE			SZ_1M
#define MX3x_CCM_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0x80000)
#define MX3x_GPT1_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0x90000)
#define MX3x_EPIT1_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0x94000)
#define MX3x_EPIT2_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0x98000)
#define MX3x_GPIO3_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xa4000)
#define MX3x_SCC_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xac000)
#define MX3x_RNGA_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xb0000)
#define MX3x_IPU_CTRL_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xc0000)
#define MX3x_AUDMUX_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xc4000)
#define MX3x_GPIO1_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xcc000)
#define MX3x_GPIO2_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xd0000)
#define MX3x_SDMA_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xd4000)
#define MX3x_RTC_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xd8000)
#define MX3x_WDOG_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xdc000)
#define MX3x_PWM_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xe0000)
#define MX3x_RTIC_BASE_ADDR			(MX3x_AIPS2_BASE_ADDR + 0xec000)

/*
 * ROMP and AVIC
 */
#define MX3x_ROMP_BASE_ADDR		0x60000000
#define MX3x_ROMP_BASE_ADDR_VIRT	0xfc500000
#define MX3x_ROMP_SIZE			SZ_1M

#define MX3x_AVIC_BASE_ADDR		0x68000000
#define MX3x_AVIC_BASE_ADDR_VIRT	0xfc400000
#define MX3x_AVIC_SIZE			SZ_1M

/*
 * Memory regions and CS
 */
#define MX3x_IPU_MEM_BASE_ADDR		0x70000000
#define MX3x_CSD0_BASE_ADDR		0x80000000
#define MX3x_CSD1_BASE_ADDR		0x90000000

#define MX3x_CS0_BASE_ADDR		0xa0000000
#define MX3x_CS1_BASE_ADDR		0xa8000000
#define MX3x_CS2_BASE_ADDR		0xb0000000
#define MX3x_CS3_BASE_ADDR		0xb2000000

#define MX3x_CS4_BASE_ADDR		0xb4000000
#define MX3x_CS4_BASE_ADDR_VIRT		0xf4000000
#define MX3x_CS4_SIZE			SZ_32M

#define MX3x_CS5_BASE_ADDR		0xb6000000
#define MX3x_CS5_BASE_ADDR_VIRT		0xf6000000
#define MX3x_CS5_SIZE			SZ_32M

/*
 * NAND, SDRAM, WEIM, M3IF, EMI controllers
 */
#define MX3x_X_MEMC_BASE_ADDR		0xb8000000
#define MX3x_X_MEMC_BASE_ADDR_VIRT	0xfc320000
#define MX3x_X_MEMC_SIZE		SZ_64K
#define MX3x_ESDCTL_BASE_ADDR			(MX3x_X_MEMC_BASE_ADDR + 0x1000)
#define MX3x_WEIM_BASE_ADDR			(MX3x_X_MEMC_BASE_ADDR + 0x2000)
#define MX3x_M3IF_BASE_ADDR			(MX3x_X_MEMC_BASE_ADDR + 0x3000)
#define MX3x_EMI_CTL_BASE_ADDR			(MX3x_X_MEMC_BASE_ADDR + 0x4000)
#define MX3x_PCMCIA_CTL_BASE_ADDR		MX3x_EMI_CTL_BASE_ADDR

#define MX3x_PCMCIA_MEM_BASE_ADDR	0xbc000000

/*!
 * This macro defines the physical to virtual address mapping for all the
 * peripheral modules. It is used by passing in the physical address as x
 * and returning the virtual address. If the physical address is not mapped,
 * it returns 0xDEADBEEF
 */
#define IO_ADDRESS(x)   \
	(void __force __iomem *) \
	(((x >= AIPS1_BASE_ADDR) && (x < (AIPS1_BASE_ADDR + AIPS1_SIZE))) ? AIPS1_IO_ADDRESS(x):\
	((x >= SPBA0_BASE_ADDR) && (x < (SPBA0_BASE_ADDR + SPBA0_SIZE))) ? SPBA0_IO_ADDRESS(x):\
	((x >= AIPS2_BASE_ADDR) && (x < (AIPS2_BASE_ADDR + AIPS2_SIZE))) ? AIPS2_IO_ADDRESS(x):\
	((x >= ROMP_BASE_ADDR) && (x < (ROMP_BASE_ADDR + ROMP_SIZE))) ? ROMP_IO_ADDRESS(x):\
	((x >= AVIC_BASE_ADDR) && (x < (AVIC_BASE_ADDR + AVIC_SIZE))) ? AVIC_IO_ADDRESS(x):\
	((x >= CS4_BASE_ADDR) && (x < (CS4_BASE_ADDR + CS4_SIZE))) ? CS4_IO_ADDRESS(x):\
	((x >= X_MEMC_BASE_ADDR) && (x < (X_MEMC_BASE_ADDR + X_MEMC_SIZE))) ? X_MEMC_IO_ADDRESS(x):\
	0xDEADBEEF)

/*
 * define the address mapping macros: in physical address order
 */
#define L2CC_IO_ADDRESS(x)  \
	(((x) - L2CC_BASE_ADDR) + L2CC_BASE_ADDR_VIRT)

#define AIPS1_IO_ADDRESS(x)  \
	(((x) - AIPS1_BASE_ADDR) + AIPS1_BASE_ADDR_VIRT)

#define SPBA0_IO_ADDRESS(x)  \
	(((x) - SPBA0_BASE_ADDR) + SPBA0_BASE_ADDR_VIRT)

#define AIPS2_IO_ADDRESS(x)  \
	(((x) - AIPS2_BASE_ADDR) + AIPS2_BASE_ADDR_VIRT)

#define ROMP_IO_ADDRESS(x)  \
	(((x) - ROMP_BASE_ADDR) + ROMP_BASE_ADDR_VIRT)

#define AVIC_IO_ADDRESS(x)  \
	(((x) - AVIC_BASE_ADDR) + AVIC_BASE_ADDR_VIRT)

#define CS4_IO_ADDRESS(x)  \
	(((x) - CS4_BASE_ADDR) + CS4_BASE_ADDR_VIRT)

#define CS5_IO_ADDRESS(x)  \
	(((x) - CS5_BASE_ADDR) + CS5_BASE_ADDR_VIRT)

#define X_MEMC_IO_ADDRESS(x)  \
	(((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

#define PCMCIA_IO_ADDRESS(x) \
	(((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

/*
 * Interrupt numbers
 */
#define MX3x_INT_I2C3		3
#define MX3x_INT_I2C2		4
#define MX3x_INT_RTIC		6
#define MX3x_INT_I2C		10
#define MX3x_INT_CSPI2		13
#define MX3x_INT_CSPI1		14
#define MX3x_INT_ATA		15
#define MX3x_INT_UART3		18
#define MX3x_INT_IIM		19
#define MX3x_INT_RNGA		22
#define MX3x_INT_EVTMON		23
#define MX3x_INT_KPP		24
#define MX3x_INT_RTC		25
#define MX3x_INT_PWM		26
#define MX3x_INT_EPIT2		27
#define MX3x_INT_EPIT1		28
#define MX3x_INT_GPT		29
#define MX3x_INT_POWER_FAIL	30
#define MX3x_INT_UART2		32
#define MX3x_INT_NANDFC		33
#define MX3x_INT_SDMA		34
#define MX3x_INT_MSHC1		39
#define MX3x_INT_IPU_ERR	41
#define MX3x_INT_IPU_SYN	42
#define MX3x_INT_UART1		45
#define MX3x_INT_ECT		48
#define MX3x_INT_SCC_SCM	49
#define MX3x_INT_SCC_SMN	50
#define MX3x_INT_GPIO2		51
#define MX3x_INT_GPIO1		52
#define MX3x_INT_WDOG		55
#define MX3x_INT_GPIO3		56
#define MX3x_INT_EXT_POWER	58
#define MX3x_INT_EXT_TEMPER	59
#define MX3x_INT_EXT_SENSOR60	60
#define MX3x_INT_EXT_SENSOR61	61
#define MX3x_INT_EXT_WDOG	62
#define MX3x_INT_EXT_TV		63

#define MX3x_PROD_SIGNATURE		0x1	/* For MX31 */

/* silicon revisions specific to i.MX31 */
#define MX3x_CHIP_REV_1_0		0x10
#define MX3x_CHIP_REV_1_1		0x11
#define MX3x_CHIP_REV_1_2		0x12
#define MX3x_CHIP_REV_1_3		0x13
#define MX3x_CHIP_REV_2_0		0x20
#define MX3x_CHIP_REV_2_1		0x21
#define MX3x_CHIP_REV_2_2		0x22
#define MX3x_CHIP_REV_2_3		0x23
#define MX3x_CHIP_REV_3_0		0x30
#define MX3x_CHIP_REV_3_1		0x31
#define MX3x_CHIP_REV_3_2		0x32

#define MX3x_SYSTEM_REV_MIN		MX3x_CHIP_REV_1_0
#define MX3x_SYSTEM_REV_NUM		3

/* Mandatory defines used globally */

#if !defined(__ASSEMBLY__) && !defined(__MXC_BOOT_UNCOMPRESS)

extern unsigned int system_rev;

static inline int mx31_revision(void)
{
	return system_rev;
}
#endif

/* these should go away */
#define L2CC_BASE_ADDR MX3x_L2CC_BASE_ADDR
#define L2CC_SIZE MX3x_L2CC_SIZE
#define AIPS1_BASE_ADDR MX3x_AIPS1_BASE_ADDR
#define AIPS1_BASE_ADDR_VIRT MX3x_AIPS1_BASE_ADDR_VIRT
#define AIPS1_SIZE MX3x_AIPS1_SIZE
#define MAX_BASE_ADDR MX3x_MAX_BASE_ADDR
#define EVTMON_BASE_ADDR MX3x_EVTMON_BASE_ADDR
#define CLKCTL_BASE_ADDR MX3x_CLKCTL_BASE_ADDR
#define ETB_SLOT4_BASE_ADDR MX3x_ETB_SLOT4_BASE_ADDR
#define ETB_SLOT5_BASE_ADDR MX3x_ETB_SLOT5_BASE_ADDR
#define ECT_CTIO_BASE_ADDR MX3x_ECT_CTIO_BASE_ADDR
#define I2C_BASE_ADDR MX3x_I2C_BASE_ADDR
#define I2C3_BASE_ADDR MX3x_I2C3_BASE_ADDR
#define UART1_BASE_ADDR MX3x_UART1_BASE_ADDR
#define UART2_BASE_ADDR MX3x_UART2_BASE_ADDR
#define I2C2_BASE_ADDR MX3x_I2C2_BASE_ADDR
#define OWIRE_BASE_ADDR MX3x_OWIRE_BASE_ADDR
#define SSI1_BASE_ADDR MX3x_SSI1_BASE_ADDR
#define CSPI1_BASE_ADDR MX3x_CSPI1_BASE_ADDR
#define KPP_BASE_ADDR MX3x_KPP_BASE_ADDR
#define IOMUXC_BASE_ADDR MX3x_IOMUXC_BASE_ADDR
#define ECT_IP1_BASE_ADDR MX3x_ECT_IP1_BASE_ADDR
#define ECT_IP2_BASE_ADDR MX3x_ECT_IP2_BASE_ADDR
#define SPBA0_BASE_ADDR MX3x_SPBA0_BASE_ADDR
#define SPBA0_BASE_ADDR_VIRT MX3x_SPBA0_BASE_ADDR_VIRT
#define SPBA0_SIZE MX3x_SPBA0_SIZE
#define UART3_BASE_ADDR MX3x_UART3_BASE_ADDR
#define CSPI2_BASE_ADDR MX3x_CSPI2_BASE_ADDR
#define SSI2_BASE_ADDR MX3x_SSI2_BASE_ADDR
#define ATA_DMA_BASE_ADDR MX3x_ATA_DMA_BASE_ADDR
#define MSHC1_BASE_ADDR MX3x_MSHC1_BASE_ADDR
#define SPBA_CTRL_BASE_ADDR MX3x_SPBA_CTRL_BASE_ADDR
#define AIPS2_BASE_ADDR MX3x_AIPS2_BASE_ADDR
#define AIPS2_BASE_ADDR_VIRT MX3x_AIPS2_BASE_ADDR_VIRT
#define AIPS2_SIZE MX3x_AIPS2_SIZE
#define CCM_BASE_ADDR MX3x_CCM_BASE_ADDR
#define GPT1_BASE_ADDR MX3x_GPT1_BASE_ADDR
#define EPIT1_BASE_ADDR MX3x_EPIT1_BASE_ADDR
#define EPIT2_BASE_ADDR MX3x_EPIT2_BASE_ADDR
#define GPIO3_BASE_ADDR MX3x_GPIO3_BASE_ADDR
#define SCC_BASE_ADDR MX3x_SCC_BASE_ADDR
#define RNGA_BASE_ADDR MX3x_RNGA_BASE_ADDR
#define IPU_CTRL_BASE_ADDR MX3x_IPU_CTRL_BASE_ADDR
#define AUDMUX_BASE_ADDR MX3x_AUDMUX_BASE_ADDR
#define GPIO1_BASE_ADDR MX3x_GPIO1_BASE_ADDR
#define GPIO2_BASE_ADDR MX3x_GPIO2_BASE_ADDR
#define SDMA_BASE_ADDR MX3x_SDMA_BASE_ADDR
#define RTC_BASE_ADDR MX3x_RTC_BASE_ADDR
#define WDOG_BASE_ADDR MX3x_WDOG_BASE_ADDR
#define PWM_BASE_ADDR MX3x_PWM_BASE_ADDR
#define RTIC_BASE_ADDR MX3x_RTIC_BASE_ADDR
#define ROMP_BASE_ADDR MX3x_ROMP_BASE_ADDR
#define ROMP_BASE_ADDR_VIRT MX3x_ROMP_BASE_ADDR_VIRT
#define ROMP_SIZE MX3x_ROMP_SIZE
#define AVIC_BASE_ADDR MX3x_AVIC_BASE_ADDR
#define AVIC_BASE_ADDR_VIRT MX3x_AVIC_BASE_ADDR_VIRT
#define AVIC_SIZE MX3x_AVIC_SIZE
#define IPU_MEM_BASE_ADDR MX3x_IPU_MEM_BASE_ADDR
#define CSD0_BASE_ADDR MX3x_CSD0_BASE_ADDR
#define CSD1_BASE_ADDR MX3x_CSD1_BASE_ADDR
#define CS0_BASE_ADDR MX3x_CS0_BASE_ADDR
#define CS1_BASE_ADDR MX3x_CS1_BASE_ADDR
#define CS2_BASE_ADDR MX3x_CS2_BASE_ADDR
#define CS3_BASE_ADDR MX3x_CS3_BASE_ADDR
#define CS4_BASE_ADDR MX3x_CS4_BASE_ADDR
#define CS4_BASE_ADDR_VIRT MX3x_CS4_BASE_ADDR_VIRT
#define CS4_SIZE MX3x_CS4_SIZE
#define CS5_BASE_ADDR MX3x_CS5_BASE_ADDR
#define CS5_BASE_ADDR_VIRT MX3x_CS5_BASE_ADDR_VIRT
#define CS5_SIZE MX3x_CS5_SIZE
#define X_MEMC_BASE_ADDR MX3x_X_MEMC_BASE_ADDR
#define X_MEMC_BASE_ADDR_VIRT MX3x_X_MEMC_BASE_ADDR_VIRT
#define X_MEMC_SIZE MX3x_X_MEMC_SIZE
#define ESDCTL_BASE_ADDR MX3x_ESDCTL_BASE_ADDR
#define WEIM_BASE_ADDR MX3x_WEIM_BASE_ADDR
#define M3IF_BASE_ADDR MX3x_M3IF_BASE_ADDR
#define EMI_CTL_BASE_ADDR MX3x_EMI_CTL_BASE_ADDR
#define PCMCIA_CTL_BASE_ADDR MX3x_PCMCIA_CTL_BASE_ADDR
#define PCMCIA_MEM_BASE_ADDR MX3x_PCMCIA_MEM_BASE_ADDR
#define MXC_INT_I2C3 MX3x_INT_I2C3
#define MXC_INT_I2C2 MX3x_INT_I2C2
#define MXC_INT_RTIC MX3x_INT_RTIC
#define MXC_INT_I2C MX3x_INT_I2C
#define MXC_INT_CSPI2 MX3x_INT_CSPI2
#define MXC_INT_CSPI1 MX3x_INT_CSPI1
#define MXC_INT_ATA MX3x_INT_ATA
#define MXC_INT_UART3 MX3x_INT_UART3
#define MXC_INT_IIM MX3x_INT_IIM
#define MXC_INT_RNGA MX3x_INT_RNGA
#define MXC_INT_EVTMON MX3x_INT_EVTMON
#define MXC_INT_KPP MX3x_INT_KPP
#define MXC_INT_RTC MX3x_INT_RTC
#define MXC_INT_PWM MX3x_INT_PWM
#define MXC_INT_EPIT2 MX3x_INT_EPIT2
#define MXC_INT_EPIT1 MX3x_INT_EPIT1
#define MXC_INT_GPT MX3x_INT_GPT
#define MXC_INT_POWER_FAIL MX3x_INT_POWER_FAIL
#define MXC_INT_UART2 MX3x_INT_UART2
#define MXC_INT_NANDFC MX3x_INT_NANDFC
#define MXC_INT_SDMA MX3x_INT_SDMA
#define MXC_INT_MSHC1 MX3x_INT_MSHC1
#define MXC_INT_IPU_ERR MX3x_INT_IPU_ERR
#define MXC_INT_IPU_SYN MX3x_INT_IPU_SYN
#define MXC_INT_UART1 MX3x_INT_UART1
#define MXC_INT_ECT MX3x_INT_ECT
#define MXC_INT_SCC_SCM MX3x_INT_SCC_SCM
#define MXC_INT_SCC_SMN MX3x_INT_SCC_SMN
#define MXC_INT_GPIO2 MX3x_INT_GPIO2
#define MXC_INT_GPIO1 MX3x_INT_GPIO1
#define MXC_INT_WDOG MX3x_INT_WDOG
#define MXC_INT_GPIO3 MX3x_INT_GPIO3
#define MXC_INT_EXT_POWER MX3x_INT_EXT_POWER
#define MXC_INT_EXT_TEMPER MX3x_INT_EXT_TEMPER
#define MXC_INT_EXT_SENSOR60 MX3x_INT_EXT_SENSOR60
#define MXC_INT_EXT_SENSOR61 MX3x_INT_EXT_SENSOR61
#define MXC_INT_EXT_WDOG MX3x_INT_EXT_WDOG
#define MXC_INT_EXT_TV MX3x_INT_EXT_TV
#define PROD_SIGNATURE MX3x_PROD_SIGNATURE
#define CHIP_REV_1_0 MX3x_CHIP_REV_1_0
#define CHIP_REV_1_1 MX3x_CHIP_REV_1_1
#define CHIP_REV_1_2 MX3x_CHIP_REV_1_2
#define CHIP_REV_1_3 MX3x_CHIP_REV_1_3
#define CHIP_REV_2_0 MX3x_CHIP_REV_2_0
#define CHIP_REV_2_1 MX3x_CHIP_REV_2_1
#define CHIP_REV_2_2 MX3x_CHIP_REV_2_2
#define CHIP_REV_2_3 MX3x_CHIP_REV_2_3
#define CHIP_REV_3_0 MX3x_CHIP_REV_3_0
#define CHIP_REV_3_1 MX3x_CHIP_REV_3_1
#define CHIP_REV_3_2 MX3x_CHIP_REV_3_2
#define SYSTEM_REV_MIN MX3x_SYSTEM_REV_MIN
#define SYSTEM_REV_NUM MX3x_SYSTEM_REV_NUM

#endif /*  __ASM_ARCH_MXC_MX31_H__ */
