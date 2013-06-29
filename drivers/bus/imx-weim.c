/*
 * EIM driver for Freescale's i.MX chips
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of_device.h>

static const struct of_device_id weim_id_table[] = {
	{ .compatible = "fsl,imx6q-weim", },
	{}
};
MODULE_DEVICE_TABLE(of, weim_id_table);

#define CS_TIMING_LEN 6
#define CS_REG_RANGE  0x18

/* Parse and set the timing for this device. */
static int __init weim_timing_setup(struct device_node *np, void __iomem *base)
{
	u32 value[CS_TIMING_LEN];
	u32 cs_idx;
	int ret;
	int i;

	/* get the CS index from this child node's "reg" property. */
	ret = of_property_read_u32(np, "reg", &cs_idx);
	if (ret)
		return ret;

	/* The weim has four chip selects. */
	if (cs_idx > 3)
		return -EINVAL;

	ret = of_property_read_u32_array(np, "fsl,weim-cs-timing",
					value, CS_TIMING_LEN);
	if (ret)
		return ret;

	/* set the timing for WEIM */
	for (i = 0; i < CS_TIMING_LEN; i++)
		writel(value[i], base + cs_idx * CS_REG_RANGE + i * 4);
	return 0;
}

static int __init weim_parse_dt(struct platform_device *pdev,
				void __iomem *base)
{
	struct device_node *child;
	int ret;

	for_each_child_of_node(pdev->dev.of_node, child) {
		if (!child->name)
			continue;

		ret = weim_timing_setup(child, base);
		if (ret) {
			dev_err(&pdev->dev, "%s set timing failed.\n",
				child->full_name);
			return ret;
		}
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "%s fail to create devices.\n",
			pdev->dev.of_node->full_name);
	return ret;
}

static int __init weim_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *clk;
	void __iomem *base;
	int ret;

	/* get the resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/* get the clock */
	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;

	/* parse the device node */
	ret = weim_parse_dt(pdev, base);
	if (ret)
		clk_disable_unprepare(clk);
	else
		dev_info(&pdev->dev, "Driver registered.\n");

	return ret;
}

static struct platform_driver weim_driver = {
	.driver = {
		.name = "imx-weim",
		.of_match_table = weim_id_table,
	},
};
module_platform_driver_probe(weim_driver, weim_probe);

MODULE_AUTHOR("Freescale Semiconductor Inc.");
MODULE_DESCRIPTION("i.MX EIM Controller Driver");
MODULE_LICENSE("GPL");
