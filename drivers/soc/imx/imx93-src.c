// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2022 NXP
 */

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define IMX93_SRC_GPR1		0x54

struct imx93_src {
	u32 reset_cause;
};

static const char * const imx93_reset_causes[] = {
	"POR",
	"JTAG",
	"IPP USER",
	"WDOG1",
	"WDOG2",
	"WDOG3",
	"WDOG4",
	"WDOG5",
	"TEMPSENSE",
	"CSU",
	"JTAG_SW",
	"M33_REQ",
	"M33_LOCKUP",
	"UNK13",
	"UNK14",
	"UNK15",
};

static const char *imx93_src_reset_cause_name(u32 cause)
{
	int i;

	for (i = ARRAY_SIZE(imx93_reset_causes) - 1; i >= 0; i--) {
		if (cause & BIT(i))
			return imx93_reset_causes[i];
	}

	return "unknown reset";
}

static int imx93_src_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct imx93_src *src;
	struct regmap *regmap;
	unsigned int cause;
	int ret;

	src = devm_kzalloc(dev, sizeof(*src), GFP_KERNEL);
	if (!src)
		return -ENOMEM;

	regmap = syscon_node_to_regmap(dev->of_node);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap),
				     "failed to get SRC regmap\n");

	ret = regmap_read(regmap, IMX93_SRC_GPR1, &cause);
	if (!ret) {
		src->reset_cause = cause;
		dev_info(dev, "reset cause: %s (0x%08x)\n",
			 imx93_src_reset_cause_name(src->reset_cause),
			 src->reset_cause);
	} else {
		dev_warn(dev, "failed to read SRC_GPR1: %d\n", ret);
	}

	platform_set_drvdata(pdev, src);

	return devm_of_platform_populate(&pdev->dev);
}

static const struct of_device_id imx93_src_ids[] = {
	{ .compatible = "fsl,imx93-src" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx93_src_ids);

static struct platform_driver imx93_src_driver = {
	.driver = {
		.name	= "imx93_src",
		.of_match_table = imx93_src_ids,
	},
	.probe = imx93_src_probe,
};
module_platform_driver(imx93_src_driver);

MODULE_AUTHOR("Peng Fan <peng.fan@nxp.com>");
MODULE_DESCRIPTION("NXP i.MX93 src driver");
MODULE_LICENSE("GPL");
