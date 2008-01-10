/*
 *   pata-legacy.c - Legacy port PATA/SATA controller driver.
 *   Copyright 2005/2006 Red Hat <alan@redhat.com>, all rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   An ATA driver for the legacy ATA ports.
 *
 *   Data Sources:
 *	Opti 82C465/82C611 support: Data sheets at opti-inc.com
 *	HT6560 series:
 *	Promise 20230/20620:
 *		http://www.ryston.cz/petr/vlb/pdc20230b.html
 *		http://www.ryston.cz/petr/vlb/pdc20230c.html
 *		http://www.ryston.cz/petr/vlb/pdc20630.html
 *
 *  Unsupported but docs exist:
 *	Appian/Adaptec AIC25VL01/Cirrus Logic PD7220
 *	Winbond W83759A
 *
 *  This driver handles legacy (that is "ISA/VLB side") IDE ports found
 *  on PC class systems. There are three hybrid devices that are exceptions
 *  The Cyrix 5510/5520 where a pre SFF ATA device is on the bridge and
 *  the MPIIX where the tuning is PCI side but the IDE is "ISA side".
 *
 *  Specific support is included for the ht6560a/ht6560b/opti82c611a/
 *  opti82c465mv/promise 20230c/20630
 *
 *  Use the autospeed and pio_mask options with:
 *	Appian ADI/2 aka CLPD7220 or AIC25VL01.
 *  Use the jumpers, autospeed and set pio_mask to the mode on the jumpers with
 *	Goldstar GM82C711, PIC-1288A-125, UMC 82C871F, Winbond W83759,
 *	Winbond W83759A, Promise PDC20230-B
 *
 *  For now use autospeed and pio_mask as above with the W83759A. This may
 *  change.
 *
 *  TODO
 *	Merge existing pata_qdi driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/ata.h>
#include <linux/libata.h>
#include <linux/platform_device.h>

#define DRV_NAME "pata_legacy"
#define DRV_VERSION "0.5.5"

#define NR_HOST 6

static int all;
module_param(all, int, 0444);
MODULE_PARM_DESC(all, "Grab all legacy port devices, even if PCI(0=off, 1=on)");

struct legacy_data {
	unsigned long timing;
	u8 clock[2];
	u8 last;
	int fast;
	struct platform_device *platform_dev;

};

enum controller {
	BIOS = 0,
	SNOOP = 1,
	PDC20230 = 2,
	HT6560A = 3,
	HT6560B = 4,
	OPTI611A = 5,
	OPTI46X = 6,
	QDI6500 = 7,
	QDI6580 = 8,
	QDI6580DP = 9,		/* Dual channel mode is different */

	UNKNOWN = -1
};


struct legacy_probe {
	unsigned char *name;
	unsigned long port;
	unsigned int irq;
	unsigned int slot;
	enum controller type;
	unsigned long private;
};

struct legacy_controller {
	const char *name;
	struct ata_port_operations *ops;
	unsigned int pio_mask;
	unsigned int flags;
	int (*setup)(struct legacy_probe *probe, struct legacy_data *data);
};

static int legacy_port[NR_HOST] = { 0x1f0, 0x170, 0x1e8, 0x168, 0x1e0, 0x160 };

static struct legacy_probe probe_list[NR_HOST];
static struct legacy_data legacy_data[NR_HOST];
static struct ata_host *legacy_host[NR_HOST];
static int nr_legacy_host;


static int probe_all;		/* Set to check all ISA port ranges */
static int ht6560a;		/* HT 6560A on primary 1, second 2, both 3 */
static int ht6560b;		/* HT 6560A on primary 1, second 2, both 3 */
static int opti82c611a;		/* Opti82c611A on primary 1, sec 2, both 3 */
static int opti82c46x;		/* Opti 82c465MV present(pri/sec autodetect) */
static int qdi;			/* Set to probe QDI controllers */
static int autospeed;		/* Chip present which snoops speed changes */
static int pio_mask = 0x1F;	/* PIO range for autospeed devices */
static int iordy_mask = 0xFFFFFFFF;	/* Use iordy if available */

/**
 *	legacy_probe_add	-	Add interface to probe list
 *	@port: Controller port
 *	@irq: IRQ number
 *	@type: Controller type
 *	@private: Controller specific info
 *
 *	Add an entry into the probe list for ATA controllers. This is used
 *	to add the default ISA slots and then to build up the table
 *	further according to other ISA/VLB/Weird device scans
 *
 *	An I/O port list is used to keep ordering stable and sane, as we
 *	don't have any good way to talk about ordering otherwise
 */

static int legacy_probe_add(unsigned long port, unsigned int irq,
				enum controller type, unsigned long private)
{
	struct legacy_probe *lp = &probe_list[0];
	int i;
	struct legacy_probe *free = NULL;

	for (i = 0; i < NR_HOST; i++) {
		if (lp->port == 0 && free == NULL)
			free = lp;
		/* Matching port, or the correct slot for ordering */
		if (lp->port == port || legacy_port[i] == port) {
			free = lp;
			break;
		}
		lp++;
	}
	if (free == NULL) {
		printk(KERN_ERR "pata_legacy: Too many interfaces.\n");
		return -1;
	}
	/* Fill in the entry for later probing */
	free->port = port;
	free->irq = irq;
	free->type = type;
	free->private = private;
	return 0;
}


/**
 *	legacy_set_mode		-	mode setting
 *	@link: IDE link
 *	@unused: Device that failed when error is returned
 *
 *	Use a non standard set_mode function. We don't want to be tuned.
 *
 *	The BIOS configured everything. Our job is not to fiddle. Just use
 *	whatever PIO the hardware is using and leave it at that. When we
 *	get some kind of nice user driven API for control then we can
 *	expand on this as per hdparm in the base kernel.
 */

static int legacy_set_mode(struct ata_link *link, struct ata_device **unused)
{
	struct ata_device *dev;

	ata_link_for_each_dev(dev, link) {
		if (ata_dev_enabled(dev)) {
			ata_dev_printk(dev, KERN_INFO,
						"configured for PIO\n");
			dev->pio_mode = XFER_PIO_0;
			dev->xfer_mode = XFER_PIO_0;
			dev->xfer_shift = ATA_SHIFT_PIO;
			dev->flags |= ATA_DFLAG_PIO;
		}
	}
	return 0;
}

static struct scsi_host_template legacy_sht = {
	.module			= THIS_MODULE,
	.name			= DRV_NAME,
	.ioctl			= ata_scsi_ioctl,
	.queuecommand		= ata_scsi_queuecmd,
	.can_queue		= ATA_DEF_QUEUE,
	.this_id		= ATA_SHT_THIS_ID,
	.sg_tablesize		= LIBATA_MAX_PRD,
	.cmd_per_lun		= ATA_SHT_CMD_PER_LUN,
	.emulated		= ATA_SHT_EMULATED,
	.use_clustering		= ATA_SHT_USE_CLUSTERING,
	.proc_name		= DRV_NAME,
	.dma_boundary		= ATA_DMA_BOUNDARY,
	.slave_configure	= ata_scsi_slave_config,
	.slave_destroy		= ata_scsi_slave_destroy,
	.bios_param		= ata_std_bios_param,
};

/*
 *	These ops are used if the user indicates the hardware
 *	snoops the commands to decide on the mode and handles the
 *	mode selection "magically" itself. Several legacy controllers
 *	do this. The mode range can be set if it is not 0x1F by setting
 *	pio_mask as well.
 */

static struct ata_port_operations simple_port_ops = {
	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= ata_data_xfer_noirq,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

static struct ata_port_operations legacy_port_ops = {
	.set_mode	= legacy_set_mode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,
	.cable_detect	= ata_cable_40wire,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= ata_data_xfer_noirq,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

/*
 *	Promise 20230C and 20620 support
 *
 *	This controller supports PIO0 to PIO2. We set PIO timings
 *	conservatively to allow for 50MHz Vesa Local Bus. The 20620 DMA
 *	support is weird being DMA to controller and PIO'd to the host
 *	and not supported.
 */

static void pdc20230_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	int tries = 5;
	int pio = adev->pio_mode - XFER_PIO_0;
	u8 rt;
	unsigned long flags;

	/* Safe as UP only. Force I/Os to occur together */

	local_irq_save(flags);

	/* Unlock the control interface */
	do {
		inb(0x1F5);
		outb(inb(0x1F2) | 0x80, 0x1F2);
		inb(0x1F2);
		inb(0x3F6);
		inb(0x3F6);
		inb(0x1F2);
		inb(0x1F2);
	}
	while ((inb(0x1F2) & 0x80) && --tries);

	local_irq_restore(flags);

	outb(inb(0x1F4) & 0x07, 0x1F4);

	rt = inb(0x1F3);
	rt &= 0x07 << (3 * adev->devno);
	if (pio)
		rt |= (1 + 3 * pio) << (3 * adev->devno);

	udelay(100);
	outb(inb(0x1F2) | 0x01, 0x1F2);
	udelay(100);
	inb(0x1F5);

}

static unsigned int pdc_data_xfer_vlb(struct ata_device *dev,
			unsigned char *buf, unsigned int buflen, int rw)
{
	if (ata_id_has_dword_io(dev->id)) {
		struct ata_port *ap = dev->link->ap;
		int slop = buflen & 3;
		unsigned long flags;

		local_irq_save(flags);

		/* Perform the 32bit I/O synchronization sequence */
		ioread8(ap->ioaddr.nsect_addr);
		ioread8(ap->ioaddr.nsect_addr);
		ioread8(ap->ioaddr.nsect_addr);

		/* Now the data */
		if (rw == READ)
			ioread32_rep(ap->ioaddr.data_addr, buf, buflen >> 2);
		else
			iowrite32_rep(ap->ioaddr.data_addr, buf, buflen >> 2);

		if (unlikely(slop)) {
			u32 pad;
			if (rw == READ) {
				pad = cpu_to_le32(ioread32(ap->ioaddr.data_addr));
				memcpy(buf + buflen - slop, &pad, slop);
			} else {
				memcpy(&pad, buf + buflen - slop, slop);
				iowrite32(le32_to_cpu(pad), ap->ioaddr.data_addr);
			}
			buflen += 4 - slop;
		}
		local_irq_restore(flags);
	} else
		buflen = ata_data_xfer_noirq(dev, buf, buflen, rw);

	return buflen;
}

static struct ata_port_operations pdc20230_port_ops = {
	.set_piomode	= pdc20230_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= pdc_data_xfer_vlb,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

/*
 *	Holtek 6560A support
 *
 *	This controller supports PIO0 to PIO2 (no IORDY even though higher
 *	timings can be loaded).
 */

static void ht6560a_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	u8 active, recover;
	struct ata_timing t;

	/* Get the timing data in cycles. For now play safe at 50Mhz */
	ata_timing_compute(adev, adev->pio_mode, &t, 20000, 1000);

	active = FIT(t.active, 2, 15);
	recover = FIT(t.recover, 4, 15);

	inb(0x3E6);
	inb(0x3E6);
	inb(0x3E6);
	inb(0x3E6);

	iowrite8(recover << 4 | active, ap->ioaddr.device_addr);
	ioread8(ap->ioaddr.status_addr);
}

static struct ata_port_operations ht6560a_port_ops = {
	.set_piomode	= ht6560a_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= ata_data_xfer,	/* Check vlb/noirq */

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

/*
 *	Holtek 6560B support
 *
 *	This controller supports PIO0 to PIO4. We honour the BIOS/jumper FIFO
 *	setting unless we see an ATAPI device in which case we force it off.
 *
 *	FIXME: need to implement 2nd channel support.
 */

static void ht6560b_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	u8 active, recover;
	struct ata_timing t;

	/* Get the timing data in cycles. For now play safe at 50Mhz */
	ata_timing_compute(adev, adev->pio_mode, &t, 20000, 1000);

	active = FIT(t.active, 2, 15);
	recover = FIT(t.recover, 2, 16);
	recover &= 0x15;

	inb(0x3E6);
	inb(0x3E6);
	inb(0x3E6);
	inb(0x3E6);

	iowrite8(recover << 4 | active, ap->ioaddr.device_addr);

	if (adev->class != ATA_DEV_ATA) {
		u8 rconf = inb(0x3E6);
		if (rconf & 0x24) {
			rconf &= ~0x24;
			outb(rconf, 0x3E6);
		}
	}
	ioread8(ap->ioaddr.status_addr);
}

static struct ata_port_operations ht6560b_port_ops = {
	.set_piomode	= ht6560b_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= ata_data_xfer,    /* FIXME: Check 32bit and noirq */

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

/*
 *	Opti core chipset helpers
 */

/**
 *	opti_syscfg	-	read OPTI chipset configuration
 *	@reg: Configuration register to read
 *
 *	Returns the value of an OPTI system board configuration register.
 */

static u8 opti_syscfg(u8 reg)
{
	unsigned long flags;
	u8 r;

	/* Uniprocessor chipset and must force cycles adjancent */
	local_irq_save(flags);
	outb(reg, 0x22);
	r = inb(0x24);
	local_irq_restore(flags);
	return r;
}

/*
 *	Opti 82C611A
 *
 *	This controller supports PIO0 to PIO3.
 */

static void opti82c611a_set_piomode(struct ata_port *ap,
						struct ata_device *adev)
{
	u8 active, recover, setup;
	struct ata_timing t;
	struct ata_device *pair = ata_dev_pair(adev);
	int clock;
	int khz[4] = { 50000, 40000, 33000, 25000 };
	u8 rc;

	/* Enter configuration mode */
	ioread16(ap->ioaddr.error_addr);
	ioread16(ap->ioaddr.error_addr);
	iowrite8(3, ap->ioaddr.nsect_addr);

	/* Read VLB clock strapping */
	clock = 1000000000 / khz[ioread8(ap->ioaddr.lbah_addr) & 0x03];

	/* Get the timing data in cycles */
	ata_timing_compute(adev, adev->pio_mode, &t, clock, 1000);

	/* Setup timing is shared */
	if (pair) {
		struct ata_timing tp;
		ata_timing_compute(pair, pair->pio_mode, &tp, clock, 1000);

		ata_timing_merge(&t, &tp, &t, ATA_TIMING_SETUP);
	}

	active = FIT(t.active, 2, 17) - 2;
	recover = FIT(t.recover, 1, 16) - 1;
	setup = FIT(t.setup, 1, 4) - 1;

	/* Select the right timing bank for write timing */
	rc = ioread8(ap->ioaddr.lbal_addr);
	rc &= 0x7F;
	rc |= (adev->devno << 7);
	iowrite8(rc, ap->ioaddr.lbal_addr);

	/* Write the timings */
	iowrite8(active << 4 | recover, ap->ioaddr.error_addr);

	/* Select the right bank for read timings, also
	   load the shared timings for address */
	rc = ioread8(ap->ioaddr.device_addr);
	rc &= 0xC0;
	rc |= adev->devno;	/* Index select */
	rc |= (setup << 4) | 0x04;
	iowrite8(rc, ap->ioaddr.device_addr);

	/* Load the read timings */
	iowrite8(active << 4 | recover, ap->ioaddr.data_addr);

	/* Ensure the timing register mode is right */
	rc = ioread8(ap->ioaddr.lbal_addr);
	rc &= 0x73;
	rc |= 0x84;
	iowrite8(rc, ap->ioaddr.lbal_addr);

	/* Exit command mode */
	iowrite8(0x83,  ap->ioaddr.nsect_addr);
}


static struct ata_port_operations opti82c611a_port_ops = {
	.set_piomode	= opti82c611a_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= ata_data_xfer,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

/*
 *	Opti 82C465MV
 *
 *	This controller supports PIO0 to PIO3. Unlike the 611A the MVB
 *	version is dual channel but doesn't have a lot of unique registers.
 */

static void opti82c46x_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	u8 active, recover, setup;
	struct ata_timing t;
	struct ata_device *pair = ata_dev_pair(adev);
	int clock;
	int khz[4] = { 50000, 40000, 33000, 25000 };
	u8 rc;
	u8 sysclk;

	/* Get the clock */
	sysclk = opti_syscfg(0xAC) & 0xC0;	/* BIOS set */

	/* Enter configuration mode */
	ioread16(ap->ioaddr.error_addr);
	ioread16(ap->ioaddr.error_addr);
	iowrite8(3, ap->ioaddr.nsect_addr);

	/* Read VLB clock strapping */
	clock = 1000000000 / khz[sysclk];

	/* Get the timing data in cycles */
	ata_timing_compute(adev, adev->pio_mode, &t, clock, 1000);

	/* Setup timing is shared */
	if (pair) {
		struct ata_timing tp;
		ata_timing_compute(pair, pair->pio_mode, &tp, clock, 1000);

		ata_timing_merge(&t, &tp, &t, ATA_TIMING_SETUP);
	}

	active = FIT(t.active, 2, 17) - 2;
	recover = FIT(t.recover, 1, 16) - 1;
	setup = FIT(t.setup, 1, 4) - 1;

	/* Select the right timing bank for write timing */
	rc = ioread8(ap->ioaddr.lbal_addr);
	rc &= 0x7F;
	rc |= (adev->devno << 7);
	iowrite8(rc, ap->ioaddr.lbal_addr);

	/* Write the timings */
	iowrite8(active << 4 | recover, ap->ioaddr.error_addr);

	/* Select the right bank for read timings, also
	   load the shared timings for address */
	rc = ioread8(ap->ioaddr.device_addr);
	rc &= 0xC0;
	rc |= adev->devno;	/* Index select */
	rc |= (setup << 4) | 0x04;
	iowrite8(rc, ap->ioaddr.device_addr);

	/* Load the read timings */
	iowrite8(active << 4 | recover, ap->ioaddr.data_addr);

	/* Ensure the timing register mode is right */
	rc = ioread8(ap->ioaddr.lbal_addr);
	rc &= 0x73;
	rc |= 0x84;
	iowrite8(rc, ap->ioaddr.lbal_addr);

	/* Exit command mode */
	iowrite8(0x83,  ap->ioaddr.nsect_addr);

	/* We need to know this for quad device on the MVB */
	ap->host->private_data = ap;
}

/**
 *	opt82c465mv_qc_issue_prot	-	command issue
 *	@qc: command pending
 *
 *	Called when the libata layer is about to issue a command. We wrap
 *	this interface so that we can load the correct ATA timings. The
 *	MVB has a single set of timing registers and these are shared
 *	across channels. As there are two registers we really ought to
 *	track the last two used values as a sort of register window. For
 *	now we just reload on a channel switch. On the single channel
 *	setup this condition never fires so we do nothing extra.
 *
 *	FIXME: dual channel needs ->serialize support
 */

static unsigned int opti82c46x_qc_issue_prot(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;

	/* If timings are set and for the wrong channel (2nd test is
	   due to a libata shortcoming and will eventually go I hope) */
	if (ap->host->private_data != ap->host
	    && ap->host->private_data != NULL)
		opti82c46x_set_piomode(ap, adev);

	return ata_qc_issue_prot(qc);
}

static struct ata_port_operations opti82c46x_port_ops = {
	.set_piomode	= opti82c46x_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= opti82c46x_qc_issue_prot,

	.data_xfer	= ata_data_xfer,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

static void qdi6500_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct ata_timing t;
	struct legacy_data *qdi = ap->host->private_data;
	int active, recovery;
	u8 timing;

	/* Get the timing data in cycles */
	ata_timing_compute(adev, adev->pio_mode, &t, 30303, 1000);

	if (qdi->fast) {
		active = 8 - FIT(t.active, 1, 8);
		recovery = 18 - FIT(t.recover, 3, 18);
	} else {
		active = 9 - FIT(t.active, 2, 9);
		recovery = 15 - FIT(t.recover, 0, 15);
	}
	timing = (recovery << 4) | active | 0x08;

	qdi->clock[adev->devno] = timing;

	outb(timing, qdi->timing);
}

/**
 *	qdi6580dp_set_piomode		-	PIO setup for dual channel
 *	@ap: Port
 *	@adev: Device
 *	@irq: interrupt line
 *
 *	In dual channel mode the 6580 has one clock per channel and we have
 *	to software clockswitch in qc_issue_prot.
 */

static void qdi6580dp_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct ata_timing t;
	struct legacy_data *qdi = ap->host->private_data;
	int active, recovery;
	u8 timing;

	/* Get the timing data in cycles */
	ata_timing_compute(adev, adev->pio_mode, &t, 30303, 1000);

	if (qdi->fast) {
		active = 8 - FIT(t.active, 1, 8);
		recovery = 18 - FIT(t.recover, 3, 18);
	} else {
		active = 9 - FIT(t.active, 2, 9);
		recovery = 15 - FIT(t.recover, 0, 15);
	}
	timing = (recovery << 4) | active | 0x08;

	qdi->clock[adev->devno] = timing;

	outb(timing, qdi->timing + 2 * ap->port_no);
	/* Clear the FIFO */
	if (adev->class != ATA_DEV_ATA)
		outb(0x5F, qdi->timing + 3);
}

/**
 *	qdi6580_set_piomode		-	PIO setup for single channel
 *	@ap: Port
 *	@adev: Device
 *
 *	In single channel mode the 6580 has one clock per device and we can
 *	avoid the requirement to clock switch. We also have to load the timing
 *	into the right clock according to whether we are master or slave.
 */

static void qdi6580_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct ata_timing t;
	struct legacy_data *qdi = ap->host->private_data;
	int active, recovery;
	u8 timing;

	/* Get the timing data in cycles */
	ata_timing_compute(adev, adev->pio_mode, &t, 30303, 1000);

	if (qdi->fast) {
		active = 8 - FIT(t.active, 1, 8);
		recovery = 18 - FIT(t.recover, 3, 18);
	} else {
		active = 9 - FIT(t.active, 2, 9);
		recovery = 15 - FIT(t.recover, 0, 15);
	}
	timing = (recovery << 4) | active | 0x08;
	qdi->clock[adev->devno] = timing;
	outb(timing, qdi->timing + 2 * adev->devno);
	/* Clear the FIFO */
	if (adev->class != ATA_DEV_ATA)
		outb(0x5F, qdi->timing + 3);
}

/**
 *	qdi_qc_issue_prot	-	command issue
 *	@qc: command pending
 *
 *	Called when the libata layer is about to issue a command. We wrap
 *	this interface so that we can load the correct ATA timings.
 */

static unsigned int qdi_qc_issue_prot(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;
	struct legacy_data *qdi = ap->host->private_data;

	if (qdi->clock[adev->devno] != qdi->last) {
		if (adev->pio_mode) {
			qdi->last = qdi->clock[adev->devno];
			outb(qdi->clock[adev->devno], qdi->timing +
							2 * ap->port_no);
		}
	}
	return ata_qc_issue_prot(qc);
}

/* For the 6580 can we flip the FIFO on/off at this point ? */

static unsigned int qdi_data_xfer(struct ata_device *adev, unsigned char *buf,
					unsigned int buflen, int rw)
{
	struct ata_port *ap = adev->link->ap;
	int slop = buflen & 3;

	if (ata_id_has_dword_io(adev->id)) {
		if (rw == WRITE)
			iowrite32_rep(ap->ioaddr.data_addr, buf, buflen >> 2);
		else
			ioread32_rep(ap->ioaddr.data_addr, buf, buflen >> 2);

		if (unlikely(slop)) {
			u32 pad;
			if (rw == WRITE) {
				memcpy(&pad, buf + buflen - slop, slop);
				pad = le32_to_cpu(pad);
				iowrite32(pad, ap->ioaddr.data_addr);
			} else {
				pad = ioread32(ap->ioaddr.data_addr);
				pad = cpu_to_le32(pad);
				memcpy(buf + buflen - slop, &pad, slop);
			}
		}
		return (buflen + 3) & ~3;
	} else
		return ata_data_xfer(adev, buf, buflen, rw);
}

static struct ata_port_operations qdi6500_port_ops = {
	.set_piomode	= qdi6500_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= qdi_qc_issue_prot,

	.data_xfer	= qdi_data_xfer,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

static struct ata_port_operations qdi6580_port_ops = {
	.set_piomode	= qdi6580_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= ata_qc_issue_prot,

	.data_xfer	= qdi_data_xfer,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

static struct ata_port_operations qdi6580dp_port_ops = {
	.set_piomode	= qdi6580dp_set_piomode,

	.tf_load	= ata_tf_load,
	.tf_read	= ata_tf_read,
	.check_status 	= ata_check_status,
	.exec_command	= ata_exec_command,
	.dev_select 	= ata_std_dev_select,

	.freeze		= ata_bmdma_freeze,
	.thaw		= ata_bmdma_thaw,
	.error_handler	= ata_bmdma_error_handler,
	.post_internal_cmd = ata_bmdma_post_internal_cmd,
	.cable_detect	= ata_cable_40wire,

	.qc_prep 	= ata_qc_prep,
	.qc_issue	= qdi_qc_issue_prot,

	.data_xfer	= qdi_data_xfer,

	.irq_handler	= ata_interrupt,
	.irq_clear	= ata_bmdma_irq_clear,
	.irq_on		= ata_irq_on,

	.port_start	= ata_sff_port_start,
};

static struct legacy_controller controllers[] = {
	{"BIOS",	&legacy_port_ops, 	0x1F,
						ATA_FLAG_NO_IORDY,	NULL },
	{"Snooping", 	&simple_port_ops, 	0x1F,
						0	       ,	NULL },
	{"PDC20230",	&pdc20230_port_ops,	0x7,
						ATA_FLAG_NO_IORDY,	NULL },
	{"HT6560A",	&ht6560a_port_ops,	0x07,
						ATA_FLAG_NO_IORDY,	NULL },
	{"HT6560B",	&ht6560b_port_ops,	0x1F,
						ATA_FLAG_NO_IORDY,	NULL },
	{"OPTI82C611A",	&opti82c611a_port_ops,	0x0F,
						0	       ,	NULL },
	{"OPTI82C46X",	&opti82c46x_port_ops,	0x0F,
						0	       ,	NULL },
	{"QDI6500",	&qdi6500_port_ops,	0x07,
						ATA_FLAG_NO_IORDY,	NULL },
	{"QDI6580",	&qdi6580_port_ops,	0x1F,
						0	       ,	NULL },
	{"QDI6580DP",	&qdi6580dp_port_ops,	0x1F,
						0	       ,	NULL }
};

/**
 *	probe_chip_type		-	Discover controller
 *	@probe: Probe entry to check
 *
 *	Probe an ATA port and identify the type of controller. We don't
 *	check if the controller appears to be driveless at this point.
 */

static int probe_chip_type(struct legacy_probe *probe)
{
	int mask = 1 << probe->slot;

	if (probe->port == 0x1F0) {
		unsigned long flags;
		local_irq_save(flags);
		/* Probes */
		outb(inb(0x1F2) | 0x80, 0x1F2);
		inb(0x1F5);
		inb(0x1F2);
		inb(0x3F6);
		inb(0x3F6);
		inb(0x1F2);
		inb(0x1F2);

		if ((inb(0x1F2) & 0x80) == 0) {
			/* PDC20230c or 20630 ? */
			printk(KERN_INFO  "PDC20230-C/20630 VLB ATA controller"
							" detected.\n");
			udelay(100);
			inb(0x1F5);
			local_irq_restore(flags);
			return PDC20230;
		} else {
			outb(0x55, 0x1F2);
			inb(0x1F2);
			inb(0x1F2);
			if (inb(0x1F2) == 0x00)
				printk(KERN_INFO "PDC20230-B VLB ATA "
						     "controller detected.\n");
			local_irq_restore(flags);
			return BIOS;
		}
		local_irq_restore(flags);
	}

	if (ht6560a & mask)
		return HT6560A;
	if (ht6560b & mask)
		return HT6560B;
	if (opti82c611a & mask)
		return OPTI611A;
	if (opti82c46x & mask)
		return OPTI46X;
	if (autospeed & mask)
		return SNOOP;
	return BIOS;
}


/**
 *	legacy_init_one		-	attach a legacy interface
 *	@pl: probe record
 *
 *	Register an ISA bus IDE interface. Such interfaces are PIO and we
 *	assume do not support IRQ sharing.
 */

static __init int legacy_init_one(struct legacy_probe *probe)
{
	struct legacy_controller *controller = &controllers[probe->type];
	int pio_modes = controller->pio_mask;
	unsigned long io = probe->port;
	u32 mask = (1 << probe->slot);
	struct ata_port_operations *ops = controller->ops;
	struct legacy_data *ld = &legacy_data[probe->slot];
	struct ata_host *host = NULL;
	struct ata_port *ap;
	struct platform_device *pdev;
	struct ata_device *dev;
	void __iomem *io_addr, *ctrl_addr;
	u32 iordy = (iordy_mask & mask) ? 0: ATA_FLAG_NO_IORDY;
	int ret;

	iordy |= controller->flags;

	pdev = platform_device_register_simple(DRV_NAME, probe->slot, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	ret = -EBUSY;
	if (devm_request_region(&pdev->dev, io, 8, "pata_legacy") == NULL ||
	    devm_request_region(&pdev->dev, io + 0x0206, 1,
							"pata_legacy") == NULL)
		goto fail;

	ret = -ENOMEM;
	io_addr = devm_ioport_map(&pdev->dev, io, 8);
	ctrl_addr = devm_ioport_map(&pdev->dev, io + 0x0206, 1);
	if (!io_addr || !ctrl_addr)
		goto fail;
	if (controller->setup)
		if (controller->setup(probe, ld) < 0)
			goto fail;
	host = ata_host_alloc(&pdev->dev, 1);
	if (!host)
		goto fail;
	ap = host->ports[0];

	ap->ops = ops;
	ap->pio_mask = pio_modes;
	ap->flags |= ATA_FLAG_SLAVE_POSS | iordy;
	ap->ioaddr.cmd_addr = io_addr;
	ap->ioaddr.altstatus_addr = ctrl_addr;
	ap->ioaddr.ctl_addr = ctrl_addr;
	ata_std_ports(&ap->ioaddr);
	ap->private_data = ld;

	ata_port_desc(ap, "cmd 0x%lx ctl 0x%lx", io, io + 0x0206);

	ret = ata_host_activate(host, probe->irq, ata_interrupt, 0,
								&legacy_sht);
	if (ret)
		goto fail;
	ld->platform_dev = pdev;

	/* Nothing found means we drop the port as its probably not there */

	ret = -ENODEV;
	ata_link_for_each_dev(dev, &ap->link) {
		if (!ata_dev_absent(dev)) {
			legacy_host[probe->slot] = host;
			ld->platform_dev = pdev;
			return 0;
		}
	}
fail:
	if (host)
		ata_host_detach(host);
	/* FIXME: use devm for this */
	if (ld->timing)
		release_region(ld->timing, 2);
	platform_device_unregister(pdev);
	return ret;
}

/**
 *	legacy_check_special_cases	-	ATA special cases
 *	@p: PCI device to check
 *	@master: set this if we find an ATA master
 *	@master: set this if we find an ATA secondary
 *
 *	A small number of vendors implemented early PCI ATA interfaces
 *	on bridge logic without the ATA interface being PCI visible.
 *	Where we have a matching PCI driver we must skip the relevant
 *	device here. If we don't know about it then the legacy driver
 *	is the right driver anyway.
 */

static void legacy_check_special_cases(struct pci_dev *p, int *primary,
								int *secondary)
{
	/* Cyrix CS5510 pre SFF MWDMA ATA on the bridge */
	if (p->vendor == 0x1078 && p->device == 0x0000) {
		*primary = *secondary = 1;
		return;
	}
	/* Cyrix CS5520 pre SFF MWDMA ATA on the bridge */
	if (p->vendor == 0x1078 && p->device == 0x0002) {
		*primary = *secondary = 1;
		return;
	}
	/* Intel MPIIX - PIO ATA on non PCI side of bridge */
	if (p->vendor == 0x8086 && p->device == 0x1234) {
		u16 r;
		pci_read_config_word(p, 0x6C, &r);
		if (r & 0x8000) {
			/* ATA port enabled */
			if (r & 0x4000)
				*secondary = 1;
			else
				*primary = 1;
		}
		return;
	}
}

static __init void probe_opti_vlb(void)
{
	/* If an OPTI 82C46X is present find out where the channels are */
	static const char *optis[4] = {
		"3/463MV", "5MV",
		"5MVA", "5MVB"
	};
	u8 chans = 1;
	u8 ctrl = (opti_syscfg(0x30) & 0xC0) >> 6;

	opti82c46x = 3;	/* Assume master and slave first */
	printk(KERN_INFO DRV_NAME ": Opti 82C46%s chipset support.\n",
								optis[ctrl]);
	if (ctrl == 3)
		chans = (opti_syscfg(0x3F) & 0x20) ? 2 : 1;
	ctrl = opti_syscfg(0xAC);
	/* Check enabled and this port is the 465MV port. On the
	   MVB we may have two channels */
	if (ctrl & 8) {
		if (chans == 2) {
			legacy_probe_add(0x1F0, 14, OPTI46X, 0);
			legacy_probe_add(0x170, 15, OPTI46X, 0);
		}
		if (ctrl & 4)
			legacy_probe_add(0x170, 15, OPTI46X, 0);
		else
			legacy_probe_add(0x1F0, 14, OPTI46X, 0);
	} else
		legacy_probe_add(0x1F0, 14, OPTI46X, 0);
}

static __init void qdi65_identify_port(u8 r, u8 res, unsigned long port)
{
	static const unsigned long ide_port[2] = { 0x170, 0x1F0 };
	/* Check card type */
	if ((r & 0xF0) == 0xC0) {
		/* QD6500: single channel */
		if (r & 8) {
			/* Disabled ? */
			release_region(port, 2);
			return;
		}
		legacy_probe_add(ide_port[r & 0x01], 14 + (r & 0x01),
								QDI6500, port);
	}
	if (((r & 0xF0) == 0xA0) || (r & 0xF0) == 0x50) {
		/* QD6580: dual channel */
		if (!request_region(port + 2 , 2, "pata_qdi")) {
			release_region(port, 2);
			return;
		}
		res = inb(port + 3);
		/* Single channel mode ? */
		if (res & 1)
			legacy_probe_add(ide_port[r & 0x01], 14 + (r & 0x01),
								QDI6580, port);
		else { /* Dual channel mode */
			legacy_probe_add(0x1F0, 14, QDI6580DP, port);
			/* port + 0x02, r & 0x04 */
			legacy_probe_add(0x170, 15, QDI6580DP, port + 2);
		}
	}
}

static __init void probe_qdi_vlb(void)
{
	unsigned long flags;
	static const unsigned long qd_port[2] = { 0x30, 0xB0 };
	int i;

	/*
	 *	Check each possible QD65xx base address
	 */

	for (i = 0; i < 2; i++) {
		unsigned long port = qd_port[i];
		u8 r, res;


		if (request_region(port, 2, "pata_qdi")) {
			/* Check for a card */
			local_irq_save(flags);
			/* I have no h/w that needs this delay but it
			   is present in the historic code */
			r = inb(port);
			udelay(1);
			outb(0x19, port);
			udelay(1);
			res = inb(port);
			udelay(1);
			outb(r, port);
			udelay(1);
			local_irq_restore(flags);

			/* Fail */
			if (res == 0x19) {
				release_region(port, 2);
				continue;
			}
			/* Passes the presence test */
			r = inb(port + 1);
			udelay(1);
			/* Check port agrees with port set */
			if ((r & 2) >> 1 != i) {
				release_region(port, 2);
				continue;
			}
			qdi65_identify_port(r, res, port);
		}
	}
}

/**
 *	legacy_init		-	attach legacy interfaces
 *
 *	Attach legacy IDE interfaces by scanning the usual IRQ/port suspects.
 *	Right now we do not scan the ide0 and ide1 address but should do so
 *	for non PCI systems or systems with no PCI IDE legacy mode devices.
 *	If you fix that note there are special cases to consider like VLB
 *	drivers and CS5510/20.
 */

static __init int legacy_init(void)
{
	int i;
	int ct = 0;
	int primary = 0;
	int secondary = 0;
	int pci_present = 0;
	struct legacy_probe *pl = &probe_list[0];
	int slot = 0;

	struct pci_dev *p = NULL;

	for_each_pci_dev(p) {
		int r;
		/* Check for any overlap of the system ATA mappings. Native
		   mode controllers stuck on these addresses or some devices
		   in 'raid' mode won't be found by the storage class test */
		for (r = 0; r < 6; r++) {
			if (pci_resource_start(p, r) == 0x1f0)
				primary = 1;
			if (pci_resource_start(p, r) == 0x170)
				secondary = 1;
		}
		/* Check for special cases */
		legacy_check_special_cases(p, &primary, &secondary);

		/* If PCI bus is present then don't probe for tertiary
		   legacy ports */
		pci_present = 1;
	}

	if (primary == 0 || all)
		legacy_probe_add(0x1F0, 14, UNKNOWN, 0);
	if (secondary == 0 || all)
		legacy_probe_add(0x170, 15, UNKNOWN, 0);

	if (probe_all || !pci_present) {
		/* ISA/VLB extra ports */
		legacy_probe_add(0x1E8, 11, UNKNOWN, 0);
		legacy_probe_add(0x168, 10, UNKNOWN, 0);
		legacy_probe_add(0x1E0, 8, UNKNOWN, 0);
		legacy_probe_add(0x160, 12, UNKNOWN, 0);
	}

	if (opti82c46x)
		probe_opti_vlb();
	if (qdi)
		probe_qdi_vlb();


	for (i = 0; i < NR_HOST; i++, pl++) {
		if (pl->port == 0)
			continue;
		if (pl->type == UNKNOWN)
			pl->type = probe_chip_type(pl);
		pl->slot = slot++;
		if (legacy_init_one(pl) == 0)
			ct++;
	}
	if (ct != 0)
		return 0;
	return -ENODEV;
}

static __exit void legacy_exit(void)
{
	int i;

	for (i = 0; i < nr_legacy_host; i++) {
		struct legacy_data *ld = &legacy_data[i];
		ata_host_detach(legacy_host[i]);
		platform_device_unregister(ld->platform_dev);
		if (ld->timing)
			release_region(ld->timing, 2);
	}
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for legacy ATA");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_param(probe_all, int, 0);
module_param(autospeed, int, 0);
module_param(ht6560a, int, 0);
module_param(ht6560b, int, 0);
module_param(opti82c611a, int, 0);
module_param(opti82c46x, int, 0);
module_param(qdi, int, 0);
module_param(pio_mask, int, 0);
module_param(iordy_mask, int, 0);

module_init(legacy_init);
module_exit(legacy_exit);
