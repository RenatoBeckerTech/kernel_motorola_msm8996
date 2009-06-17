/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/528x/config.c
 *
 *	Sub-architcture dependant initialization code for the Motorola
 *	5280 and 5282 CPUs.
 *
 *	Copyright (C) 1999-2003, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 */

/***************************************************************************/

#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

/***************************************************************************/

static struct mcf_platform_uart m528x_uart_platform[] = {
	{
		.mapbase	= MCF_MBAR + MCFUART_BASE1,
		.irq		= MCFINT_VECBASE + MCFINT_UART0,
	},
	{
		.mapbase 	= MCF_MBAR + MCFUART_BASE2,
		.irq		= MCFINT_VECBASE + MCFINT_UART0 + 1,
	},
	{
		.mapbase 	= MCF_MBAR + MCFUART_BASE3,
		.irq		= MCFINT_VECBASE + MCFINT_UART0 + 2,
	},
	{ },
};

static struct platform_device m528x_uart = {
	.name			= "mcfuart",
	.id			= 0,
	.dev.platform_data	= m528x_uart_platform,
};

static struct resource m528x_fec_resources[] = {
	{
		.start		= MCF_MBAR + 0x1000,
		.end		= MCF_MBAR + 0x1000 + 0x7ff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= 64 + 23,
		.end		= 64 + 23,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= 64 + 27,
		.end		= 64 + 27,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= 64 + 29,
		.end		= 64 + 29,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device m528x_fec = {
	.name			= "fec",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(m528x_fec_resources),
	.resource		= m528x_fec_resources,
};


static struct platform_device *m528x_devices[] __initdata = {
	&m528x_uart,
	&m528x_fec,
};

/***************************************************************************/

#define	INTC0	(MCF_MBAR + MCFICM_INTC0)

static void __init m528x_uart_init_line(int line, int irq)
{
	u8 port;
	u32 imr;

	if ((line < 0) || (line > 2))
		return;

	/* level 6, line based priority */
	writeb(0x30+line, INTC0 + MCFINTC_ICR0 + MCFINT_UART0 + line);

	imr = readl(INTC0 + MCFINTC_IMRL);
	imr &= ~((1 << (irq - MCFINT_VECBASE)) | 1);
	writel(imr, INTC0 + MCFINTC_IMRL);

	/* make sure PUAPAR is set for UART0 and UART1 */
	if (line < 2) {
		port = readb(MCF_MBAR + MCF5282_GPIO_PUAPAR);
		port |= (0x03 << (line * 2));
		writeb(port, MCF_MBAR + MCF5282_GPIO_PUAPAR);
	}
}

static void __init m528x_uarts_init(void)
{
	const int nrlines = ARRAY_SIZE(m528x_uart_platform);
	int line;

	for (line = 0; (line < nrlines); line++)
		m528x_uart_init_line(line, m528x_uart_platform[line].irq);
}

/***************************************************************************/

static void __init m528x_fec_init(void)
{
	u32 imr;
	u16 v16;

	/* Unmask FEC interrupts at ColdFire interrupt controller */
	writeb(0x28, MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_ICR0 + 23);
	writeb(0x27, MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_ICR0 + 27);
	writeb(0x26, MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_ICR0 + 29);

	imr = readl(MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH);
	imr &= ~0xf;
	writel(imr, MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH);
	imr = readl(MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL);
	imr &= ~0xff800001;
	writel(imr, MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL);

	/* Set multi-function pins to ethernet mode for fec0 */
	v16 = readw(MCF_IPSBAR + 0x100056);
	writew(v16 | 0xf00, MCF_IPSBAR + 0x100056);
	writeb(0xc0, MCF_IPSBAR + 0x100058);
}

/***************************************************************************/

void mcf_disableall(void)
{
	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH)) = 0xffffffff;
	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL)) = 0xffffffff;
}

/***************************************************************************/

void mcf_autovector(unsigned int vec)
{
	/* Everything is auto-vectored on the 5272 */
}

/***************************************************************************/

static void m528x_cpu_reset(void)
{
	local_irq_disable();
	__raw_writeb(MCF_RCR_SWRESET, MCF_IPSBAR + MCF_RCR);
}

/***************************************************************************/

#ifdef CONFIG_WILDFIRE
void wildfire_halt(void)
{
	writeb(0, 0x30000007);
	writeb(0x2, 0x30000007);
}
#endif

#ifdef CONFIG_WILDFIREMOD
void wildfiremod_halt(void)
{
	printk(KERN_INFO "WildFireMod hibernating...\n");

	/* Set portE.5 to Digital IO */
	MCF5282_GPIO_PEPAR &= ~(1 << (5 * 2));

	/* Make portE.5 an output */
	MCF5282_GPIO_DDRE |= (1 << 5);

	/* Now toggle portE.5 from low to high */
	MCF5282_GPIO_PORTE &= ~(1 << 5);
	MCF5282_GPIO_PORTE |= (1 << 5);

	printk(KERN_EMERG "Failed to hibernate. Halting!\n");
}
#endif

void __init config_BSP(char *commandp, int size)
{
	mcf_disableall();

#ifdef CONFIG_WILDFIRE
	mach_halt = wildfire_halt;
#endif
#ifdef CONFIG_WILDFIREMOD
	mach_halt = wildfiremod_halt;
#endif
}

/***************************************************************************/

static int __init init_BSP(void)
{
	mach_reset = m528x_cpu_reset;
	m528x_uarts_init();
	m528x_fec_init();
	platform_add_devices(m528x_devices, ARRAY_SIZE(m528x_devices));
	return 0;
}

arch_initcall(init_BSP);

/***************************************************************************/
