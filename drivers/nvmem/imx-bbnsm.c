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
#include <linux/reboot.h>
#include <linux/regmap.h>

#define BBNSM_FEATURES		0x4
#define BBNSM_GPR_SZ_SHIFT	2
#define BBNSM_GPR_SZ_MASK	GENMASK(7, BBNSM_GPR_SZ_SHIFT)

#define BBNSM_GPR		0x300
#define BBNSM_GPR7		0x31c
#define BBNSM_REBOOT_MAGIC	0x5741524d /* "WARM" */

struct imx_bbnsm_priv {
	struct regmap		*regmap;
	struct nvmem_config	cfg;
	struct notifier_block	reboot_nb;
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


static int bbnsm_rtc_reboot(struct notifier_block *nb,
			    unsigned long action, void *data)
{
	struct imx_bbnsm_priv *priv = container_of(nb, struct imx_bbnsm_priv, reboot_nb);
	int ret;

	if (action != SYS_RESTART)
		return NOTIFY_DONE;

	ret = regmap_write(priv->regmap, BBNSM_GPR7, BBNSM_REBOOT_MAGIC);
	if (ret)
		dev_warn(priv->cfg.dev, "failed to store reboot marker: %d\n", ret);

	return NOTIFY_DONE;
}

static int imx_bbnsm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent;
	struct imx_bbnsm_priv *priv;
	struct nvmem_device *nvmem;
	u32 gpr_words;
	u32 val;
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

	ret = regmap_read(priv->regmap, BBNSM_FEATURES, &val);
	if (ret)
		return ret;

	/* Are any general purpose register available? */
	gpr_words = (val & BBNSM_GPR_SZ_MASK) >> BBNSM_GPR_SZ_SHIFT;
	if (!gpr_words)
		return -ENODEV;

	ret = regmap_read(priv->regmap, BBNSM_GPR7, &val);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read reboot marker\n");

	dev_info(dev, "BBNSM_GPR7: 0x%08x\n", val);

	priv->cfg.dev = dev;
	priv->cfg.name = dev_name(dev);
	priv->cfg.owner = THIS_MODULE;
	priv->cfg.priv = priv;
	priv->cfg.stride = 4;
	priv->cfg.word_size = 4;
	priv->cfg.size = gpr_words * sizeof(u32);
	priv->cfg.reg_read = imx_bbnsm_read;
	priv->cfg.reg_write = imx_bbnsm_write;

	priv->reboot_nb.notifier_call = bbnsm_rtc_reboot;
	ret = devm_register_reboot_notifier(dev, &priv->reboot_nb);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register reboot notifier\n");

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
