/*
 * comedi/drivers/acl7225b.c
 * Driver for Adlink NuDAQ ACL-7225b and clones
 * José Luis Sánchez
 */
/*
Driver: acl7225b
Description: Adlink NuDAQ ACL-7225b & compatibles
Author: José Luis Sánchez (jsanchezv@teleline.es)
Status: testing
Devices: [Adlink] ACL-7225b (acl7225b), [ICP] P16R16DIO (p16r16dio)
*/

#include "../comedidev.h"

#include <linux/ioport.h>

#define ACL7225_RIO_LO 0	/* Relays input/output low byte (R0-R7) */
#define ACL7225_RIO_HI 1	/* Relays input/output high byte (R8-R15) */
#define ACL7225_DI_LO  2	/* Digital input low byte (DI0-DI7) */
#define ACL7225_DI_HI  3	/* Digital input high byte (DI8-DI15) */

struct acl7225b_boardinfo {
	const char *name;
	int io_range;
};

static const struct acl7225b_boardinfo acl7225b_boards[] = {
	{
		.name		= "acl7225b",
		.io_range	= 8,		/* only 4 are used */
	}, {
		.name		= "p16r16dio",
		.io_range	= 4,
	},
};

static int acl7225b_do_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
	}
	if (data[0] & 0x00ff)
		outb(s->state & 0xff, dev->iobase + (unsigned long)s->private);
	if (data[0] & 0xff00)
		outb((s->state >> 8),
		     dev->iobase + (unsigned long)s->private + 1);

	data[1] = s->state;

	return insn->n;
}

static int acl7225b_di_insn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	data[1] = inb(dev->iobase + (unsigned long)s->private) |
	    (inb(dev->iobase + (unsigned long)s->private + 1) << 8);

	return insn->n;
}

static int acl7225b_attach(struct comedi_device *dev,
			   struct comedi_devconfig *it)
{
	const struct acl7225b_boardinfo *board = comedi_board(dev);
	struct comedi_subdevice *s;
	int iobase;
	int ret;

	iobase = it->options[0];
	if (!request_region(iobase, board->io_range, dev->board_name))
		return -EIO;
	dev->iobase = iobase;

	ret = comedi_alloc_subdevices(dev, 3);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	/* Relays outputs */
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE;
	s->maxdata = 1;
	s->n_chan = 16;
	s->insn_bits = acl7225b_do_insn;
	s->range_table = &range_digital;
	s->private = (void *)ACL7225_RIO_LO;

	s = &dev->subdevices[1];
	/* Relays status */
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE;
	s->maxdata = 1;
	s->n_chan = 16;
	s->insn_bits = acl7225b_di_insn;
	s->range_table = &range_digital;
	s->private = (void *)ACL7225_RIO_LO;

	s = &dev->subdevices[2];
	/* Isolated digital inputs */
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE;
	s->maxdata = 1;
	s->n_chan = 16;
	s->insn_bits = acl7225b_di_insn;
	s->range_table = &range_digital;
	s->private = (void *)ACL7225_DI_LO;

	return 0;
}

static void acl7225b_detach(struct comedi_device *dev)
{
	const struct acl7225b_boardinfo *board = comedi_board(dev);

	if (dev->iobase)
		release_region(dev->iobase, board->io_range);
}

static struct comedi_driver acl7225b_driver = {
	.driver_name	= "acl7225b",
	.module		= THIS_MODULE,
	.attach		= acl7225b_attach,
	.detach		= acl7225b_detach,
	.board_name	= &acl7225b_boards[0].name,
	.num_names	= ARRAY_SIZE(acl7225b_boards),
	.offset		= sizeof(struct acl7225b_boardinfo),
};
module_comedi_driver(acl7225b_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
