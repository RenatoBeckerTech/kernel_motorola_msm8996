/* arch/arm/mach-s3c2410/include/mach/regs-s3c2443-clock.h
 *
 * Copyright (c) 2007 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2443 clock register definitions
*/

#ifndef __ASM_ARM_REGS_S3C2443_CLOCK
#define __ASM_ARM_REGS_S3C2443_CLOCK

#define S3C2443_CLKREG(x)		((x) + S3C24XX_VA_CLKPWR)

#define S3C2443_PLLCON_MDIVSHIFT	16
#define S3C2443_PLLCON_PDIVSHIFT	8
#define S3C2443_PLLCON_SDIVSHIFT	0
#define S3C2443_PLLCON_MDIVMASK		((1<<(1+(23-16)))-1)
#define S3C2443_PLLCON_PDIVMASK		((1<<(1+(9-8)))-1)
#define S3C2443_PLLCON_SDIVMASK		(3)

#define S3C2443_MPLLCON			S3C2443_CLKREG(0x10)
#define S3C2443_EPLLCON			S3C2443_CLKREG(0x18)
#define S3C2443_CLKSRC			S3C2443_CLKREG(0x20)
#define S3C2443_CLKDIV0			S3C2443_CLKREG(0x24)
#define S3C2443_CLKDIV1			S3C2443_CLKREG(0x28)
#define S3C2443_HCLKCON			S3C2443_CLKREG(0x30)
#define S3C2443_PCLKCON			S3C2443_CLKREG(0x34)
#define S3C2443_SCLKCON			S3C2443_CLKREG(0x38)
#define S3C2443_PWRMODE			S3C2443_CLKREG(0x40)
#define S3C2443_SWRST			S3C2443_CLKREG(0x44)
#define S3C2443_BUSPRI0			S3C2443_CLKREG(0x50)
#define S3C2443_SYSID			S3C2443_CLKREG(0x5C)
#define S3C2443_PWRCFG			S3C2443_CLKREG(0x60)
#define S3C2443_RSTCON			S3C2443_CLKREG(0x64)
#define S3C2443_PHYCTRL			S3C2443_CLKREG(0x80)
#define S3C2443_PHYPWR			S3C2443_CLKREG(0x84)
#define S3C2443_URSTCON			S3C2443_CLKREG(0x88)
#define S3C2443_UCLKCON			S3C2443_CLKREG(0x8C)

#define S3C2443_SWRST_RESET		(0x533c2443)

#define S3C2443_PLLCON_OFF		(1<<24)

#define S3C2443_CLKSRC_EPLLREF_XTAL	(2<<7)
#define S3C2443_CLKSRC_EPLLREF_EXTCLK	(3<<7)
#define S3C2443_CLKSRC_EPLLREF_MPLLREF	(0<<7)
#define S3C2443_CLKSRC_EPLLREF_MPLLREF2	(1<<7)
#define S3C2443_CLKSRC_EPLLREF_MASK	(3<<7)

#define S3C2443_CLKSRC_EXTCLK_DIV	(1<<3)

#define S3C2443_CLKDIV0_HALF_HCLK	(1<<3)
#define S3C2443_CLKDIV0_HALF_PCLK	(1<<2)

#define S3C2443_CLKDIV0_HCLKDIV_MASK	(3<<0)

#define S3C2443_CLKDIV0_EXTDIV_MASK	(3<<6)
#define S3C2443_CLKDIV0_EXTDIV_SHIFT	(6)

#define S3C2443_CLKDIV0_PREDIV_MASK	(3<<4)
#define S3C2443_CLKDIV0_PREDIV_SHIFT	(4)

#define S3C2443_CLKDIV0_ARMDIV_MASK	(15<<9)
#define S3C2443_CLKDIV0_ARMDIV_SHIFT	(9)
#define S3C2443_CLKDIV0_ARMDIV_1	(0<<9)
#define S3C2443_CLKDIV0_ARMDIV_2	(8<<9)
#define S3C2443_CLKDIV0_ARMDIV_3	(2<<9)
#define S3C2443_CLKDIV0_ARMDIV_4	(9<<9)
#define S3C2443_CLKDIV0_ARMDIV_6	(10<<9)
#define S3C2443_CLKDIV0_ARMDIV_8	(11<<9)
#define S3C2443_CLKDIV0_ARMDIV_12	(13<<9)
#define S3C2443_CLKDIV0_ARMDIV_16	(15<<9)

/* S3C2443_CLKDIV1 removed, only used in clock.c code */

#define S3C2443_CLKCON_NAND

#define S3C2443_HCLKCON_DMA0		(1<<0)
#define S3C2443_HCLKCON_DMA1		(1<<1)
#define S3C2443_HCLKCON_DMA2		(1<<2)
#define S3C2443_HCLKCON_DMA3		(1<<3)
#define S3C2443_HCLKCON_DMA4		(1<<4)
#define S3C2443_HCLKCON_DMA5		(1<<5)
#define S3C2443_HCLKCON_CAMIF		(1<<8)
#define S3C2443_HCLKCON_LCDC		(1<<9)
#define S3C2443_HCLKCON_USBH		(1<<11)
#define S3C2443_HCLKCON_USBD		(1<<12)
#define S3C2416_HCLKCON_HSMMC0		(1<<15)
#define S3C2443_HCLKCON_HSMMC		(1<<16)
#define S3C2443_HCLKCON_CFC		(1<<17)
#define S3C2443_HCLKCON_SSMC		(1<<18)
#define S3C2443_HCLKCON_DRAMC		(1<<19)

#define S3C2443_PCLKCON_UART0		(1<<0)
#define S3C2443_PCLKCON_UART1		(1<<1)
#define S3C2443_PCLKCON_UART2		(1<<2)
#define S3C2443_PCLKCON_UART3		(1<<3)
#define S3C2443_PCLKCON_IIC		(1<<4)
#define S3C2443_PCLKCON_SDI		(1<<5)
#define S3C2443_PCLKCON_HSSPI		(1<<6)
#define S3C2443_PCLKCON_ADC		(1<<7)
#define S3C2443_PCLKCON_AC97		(1<<8)
#define S3C2443_PCLKCON_IIS		(1<<9)
#define S3C2443_PCLKCON_PWMT		(1<<10)
#define S3C2443_PCLKCON_WDT		(1<<11)
#define S3C2443_PCLKCON_RTC		(1<<12)
#define S3C2443_PCLKCON_GPIO		(1<<13)
#define S3C2443_PCLKCON_SPI0		(1<<14)
#define S3C2443_PCLKCON_SPI1		(1<<15)

#define S3C2443_SCLKCON_DDRCLK		(1<<16)
#define S3C2443_SCLKCON_SSMCCLK		(1<<15)
#define S3C2443_SCLKCON_HSSPICLK	(1<<14)
#define S3C2443_SCLKCON_HSMMCCLK_EXT	(1<<13)
#define S3C2443_SCLKCON_HSMMCCLK_EPLL	(1<<12)
#define S3C2443_SCLKCON_CAMCLK		(1<<11)
#define S3C2443_SCLKCON_DISPCLK		(1<<10)
#define S3C2443_SCLKCON_I2SCLK		(1<<9)
#define S3C2443_SCLKCON_UARTCLK		(1<<8)
#define S3C2443_SCLKCON_USBHOST		(1<<1)

#define S3C2443_PWRCFG_SLEEP		(1<<15)

#define S3C2443_PWRCFG_USBPHY		(1 << 4)

#define S3C2443_URSTCON_FUNCRST		(1 << 2)
#define S3C2443_URSTCON_PHYRST		(1 << 0)

#define S3C2443_PHYCTRL_CLKSEL		(1 << 3)
#define S3C2443_PHYCTRL_EXTCLK		(1 << 2)
#define S3C2443_PHYCTRL_PLLSEL		(1 << 1)
#define S3C2443_PHYCTRL_DSPORT		(1 << 0)

#define S3C2443_PHYPWR_COMMON_ON	(1 << 31)
#define S3C2443_PHYPWR_ANALOG_PD	(1 << 4)
#define S3C2443_PHYPWR_PLL_REFCLK	(1 << 3)
#define S3C2443_PHYPWR_XO_ON		(1 << 2)
#define S3C2443_PHYPWR_PLL_PWRDN	(1 << 1)
#define S3C2443_PHYPWR_FSUSPEND		(1 << 0)

#define S3C2443_UCLKCON_DETECT_VBUS	(1 << 31)
#define S3C2443_UCLKCON_FUNC_CLKEN	(1 << 2)
#define S3C2443_UCLKCON_TCLKEN		(1 << 0)

#include <asm/div64.h>

static inline unsigned int
s3c2443_get_mpll(unsigned int pllval, unsigned int baseclk)
{
	unsigned int mdiv, pdiv, sdiv;
	uint64_t fvco;

	mdiv = pllval >> S3C2443_PLLCON_MDIVSHIFT;
	pdiv = pllval >> S3C2443_PLLCON_PDIVSHIFT;
	sdiv = pllval >> S3C2443_PLLCON_SDIVSHIFT;

	mdiv &= S3C2443_PLLCON_MDIVMASK;
	pdiv &= S3C2443_PLLCON_PDIVMASK;
	sdiv &= S3C2443_PLLCON_SDIVMASK;

	fvco = (uint64_t)baseclk * (2 * (mdiv + 8));
	do_div(fvco, pdiv << sdiv);

	return (unsigned int)fvco;
}

static inline unsigned int
s3c2443_get_epll(unsigned int pllval, unsigned int baseclk)
{
	unsigned int mdiv, pdiv, sdiv;
	uint64_t fvco;

	mdiv = pllval >> S3C2443_PLLCON_MDIVSHIFT;
	pdiv = pllval >> S3C2443_PLLCON_PDIVSHIFT;
	sdiv = pllval >> S3C2443_PLLCON_SDIVSHIFT;

	mdiv &= S3C2443_PLLCON_MDIVMASK;
	pdiv &= S3C2443_PLLCON_PDIVMASK;
	sdiv &= S3C2443_PLLCON_SDIVMASK;

	fvco = (uint64_t)baseclk * (mdiv + 8);
	do_div(fvco, (pdiv + 2) << sdiv);

	return (unsigned int)fvco;
}

#endif /*  __ASM_ARM_REGS_S3C2443_CLOCK */

