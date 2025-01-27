#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include "../axp-core.h"
#include "../axp-powerkey.h"
#include "axp15.h"

static int axp_powerkey_probe(struct platform_device *pdev)
{
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	struct axp_powerkey_info *info;
	struct input_dev *powerkey_dev;
	int err = 0, i, irq, ret;
	struct axp_regmap *map = axp_dev->regmap;
	u8 val;

	if (pdev->dev.of_node) {
		/* get dt and sysconfig */
		ret = axp_powerkey_dt_parse(pdev->dev.of_node, &axp15_config);
		if (ret) {
			pr_err("%s parse device tree err\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("axp15 powerkey device tree err!\n");
		return -EBUSY;
	}

	info = kzalloc(sizeof(struct axp_powerkey_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = axp_dev;

	/* register input device */
	powerkey_dev = input_allocate_device();
	if (!powerkey_dev) {
		pr_err("alloc powerkey input device error\n");
		goto out;
	}

	powerkey_dev->name = pdev->name;
	powerkey_dev->phys = "m1kbd/input2";
	powerkey_dev->id.bustype = BUS_HOST;
	powerkey_dev->id.vendor = 0x0001;
	powerkey_dev->id.product = 0x0001;
	powerkey_dev->id.version = 0x0100;
	powerkey_dev->open = NULL;
	powerkey_dev->close = NULL;
	powerkey_dev->dev.parent = &pdev->dev;
	set_bit(EV_KEY, powerkey_dev->evbit);
	set_bit(EV_REL, powerkey_dev->evbit);
	set_bit(KEY_POWER, powerkey_dev->keybit);

	err = input_register_device(powerkey_dev);
	if (err) {
		pr_err("Unable to Register the axp_powerkey\n");
		goto out_reg;
	}

	info->idev = powerkey_dev;

	for (i = 0; i < ARRAY_SIZE(axp_powerkey_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp_powerkey_irq[i].name);
		if (irq < 0)
			continue;

		err = axp_request_irq(axp_dev, irq,
				axp_powerkey_irq[i].isr, info);
		if (err != 0) {
			dev_err(&pdev->dev, "failed to request %s IRQ %d: %d\n"
					, axp_powerkey_irq[i].name, irq, err);
			goto out_irq;
		}

		dev_dbg(&pdev->dev, "Requested %s IRQ %d: %d\n",
				axp_powerkey_irq[i].name, irq, err);
	}

	platform_set_drvdata(pdev, info);

	/* pok on time set */
	axp_regmap_read(map, AXP15_POK_SET, &val);
	if (axp15_config.pmu_powkey_on_time < 1000)
		val &= 0x3f;
	else if (axp15_config.pmu_powkey_on_time < 2000) {
		val &= 0x3f;
		val |= 0x80;
	} else if (axp15_config.pmu_powkey_on_time < 3000) {
		val &= 0x3f;
		val |= 0xc0;
	} else {
		val &= 0x3f;
		val |= 0x40;
	}
	axp_regmap_write(map, AXP15_POK_SET, val);

	/* pok long time set*/
	if (axp15_config.pmu_powkey_long_time < 1000)
		axp15_config.pmu_powkey_long_time = 1000;

	if (axp15_config.pmu_powkey_long_time > 2500)
		axp15_config.pmu_powkey_long_time = 2500;

	axp_regmap_read(map, AXP15_POK_SET, &val);
	val &= 0xcf;
	val |= (((axp15_config.pmu_powkey_long_time - 1000) / 500) << 4);
	axp_regmap_write(map, AXP15_POK_SET, val);

	/* pek offlevel poweroff en set*/
	if (axp15_config.pmu_powkey_off_en)
		axp15_config.pmu_powkey_off_en = 1;
	else
		axp15_config.pmu_powkey_off_en = 0;

	axp_regmap_read(map, AXP15_POK_SET, &val);
	val &= 0xf7;
	val |= (axp15_config.pmu_powkey_off_en << 3);
	axp_regmap_write(map, AXP15_POK_SET, val);

	/* pek offlevel time set */
	if (axp15_config.pmu_powkey_off_time < 4000)
		axp15_config.pmu_powkey_off_time = 4000;

	if (axp15_config.pmu_powkey_off_time > 10000)
		axp15_config.pmu_powkey_off_time = 10000;

	axp_regmap_read(map, AXP15_POK_SET, &val);
	val &= 0xfc;
	val |= (axp15_config.pmu_powkey_off_time - 4000) / 2000;
	axp_regmap_write(map, AXP15_POK_SET, val);

	return 0;

out_irq:
	for (i = i - 1; i >= 0; i--) {
		irq = platform_get_irq_byname(pdev, axp_powerkey_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}
out_reg:
	input_free_device(powerkey_dev);
out:
	kfree(info);
	return err;
}

static int axp_powerkey_remove(struct platform_device *pdev)
{
	int i, irq;
	struct axp_powerkey_info *info = platform_get_drvdata(pdev);
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);

	for (i = 0; i < ARRAY_SIZE(axp_powerkey_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp_powerkey_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}

	input_unregister_device(info->idev);
	kfree(info);

	return 0;
}

static const struct of_device_id axp_powerkey_dt_ids[] = {
	{ .compatible = "axp157-powerkey", },
	{},
};
MODULE_DEVICE_TABLE(of, axp_powerkey_dt_ids);

static struct platform_driver axp_powerkey_driver = {
	.driver = {
		.name = "axp15-powerkey",
		.of_match_table = axp_powerkey_dt_ids,
	},
	.probe  = axp_powerkey_probe,
	.remove = axp_powerkey_remove,
};

module_platform_driver(axp_powerkey_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("onkey Driver for axp15x PMIC");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");
