// SPDX-License-Identifier: GPL-2.0-only
/*
 * i.MX Battery-Backed Non-Secure Module general purpose registers
 *
 * Copyright (c) 2026 Stefan Wahren <wahrenst@gmx.net>
 *
 * Based on snvs lpgpr driver,
 * Copyright (c) 2015 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 * Copyright (c) 2017 Pengutronix, Oleksij Rempel <kernel@pengutronix.de>
 */

#include <linux/bitfield.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define BBNSM_FEATURES		0x4
#define BBNSM_GPR_SZ_SHIFT	2
#define BBNSM_GPR_SZ_MASK	GENMASK(7, BBNSM_GPR_SZ_SHIFT)

#define BBNSM_GPR		0x300

struct imx_bbnsm_priv {
	struct regmap		*regmap;
	struct nvmem_config	cfg;
};

static int imx_bbnsm_read(void *context, unsigned int offset, void *val,
			  size_t bytes)
{
	struct imx_bbnsm_priv *priv = context;

	return regmap_bulk_read(priv->regmap, BBNSM_GPR + offset, val,
				bytes / 4);
}

static int imx_bbnsm_write(void *context, unsigned int offset, void *val,
			   size_t bytes)
{
	struct imx_bbnsm_priv *priv = context;

	return regmap_bulk_write(priv->regmap, BBNSM_GPR + offset, val,
				 bytes / 4);
}

static int imx_bbnsm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent;
	struct imx_bbnsm_priv *priv;
	struct nvmem_device *nvmem;
	u32 gpr_words;
	u32 features;
	int ret;

	if (!dev->of_node)
		return -ENOENT;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	parent = of_get_parent(dev->of_node);
	if (!parent)
		return -ENODEV;

	priv->regmap = syscon_node_to_regmap(parent);
	of_node_put(parent);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	ret = regmap_read(priv->regmap, BBNSM_FEATURES, &features);
	if (ret)
		return ret;

	/* Are any general purpose register available? */
	gpr_words = (features & BBNSM_GPR_SZ_MASK) >> BBNSM_GPR_SZ_SHIFT;
	if (!gpr_words)
		return -ENODEV;

	priv->cfg.dev = dev;
	priv->cfg.name = dev_name(dev);
	priv->cfg.owner = THIS_MODULE;
	priv->cfg.priv = priv;
	priv->cfg.stride = 4;
	priv->cfg.word_size = 4;
	priv->cfg.size = gpr_words * sizeof(u32);
	priv->cfg.reg_read = imx_bbnsm_read;
	priv->cfg.reg_write = imx_bbnsm_write;

	nvmem = devm_nvmem_register(dev, &priv->cfg);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id imx_bbnsm_of_match[] = {
	{ .compatible = "nxp,imx93-bbnsm-nvmem" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx_bbnsm_of_match);

static struct platform_driver imx_bbnsm_driver = {
	.probe = imx_bbnsm_probe,
	.driver = {
		.name = "imx-bbnsm-nvmem",
		.of_match_table = imx_bbnsm_of_match,
	},
};
module_platform_driver(imx_bbnsm_driver);

MODULE_AUTHOR("Stefan Wahren <wahrenst@gmx.net>");
MODULE_DESCRIPTION("i.MX BBNSM general purpose register NVMEM driver");
MODULE_LICENSE("GPL");
