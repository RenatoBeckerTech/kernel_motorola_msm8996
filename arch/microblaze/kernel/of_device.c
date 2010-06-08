#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#include <linux/errno.h>

void of_device_make_bus_id(struct of_device *dev)
{
	static atomic_t bus_no_reg_magic;
	struct device_node *node = dev->dev.of_node;
	const u32 *reg;
	u64 addr;
	int magic;

	/*
	 * For MMIO, get the physical address
	 */
	reg = of_get_property(node, "reg", NULL);
	if (reg) {
		addr = of_translate_address(node, reg);
		if (addr != OF_BAD_ADDR) {
			dev_set_name(&dev->dev, "%llx.%s",
				     (unsigned long long)addr, node->name);
			return;
		}
	}

	/*
	 * No BusID, use the node name and add a globally incremented
	 * counter (and pray...)
	 */
	magic = atomic_add_return(1, &bus_no_reg_magic);
	dev_set_name(&dev->dev, "%s.%d", node->name, magic - 1);
}
EXPORT_SYMBOL(of_device_make_bus_id);

struct of_device *of_device_alloc(struct device_node *np,
				  const char *bus_id,
				  struct device *parent)
{
	struct of_device *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	dev->dev.of_node = of_node_get(np);
	dev->dev.dma_mask = &dev->archdata.dma_mask;
	dev->dev.parent = parent;
	dev->dev.release = of_release_dev;

	if (bus_id)
		dev_set_name(&dev->dev, bus_id);
	else
		of_device_make_bus_id(dev);

	return dev;
}
EXPORT_SYMBOL(of_device_alloc);
