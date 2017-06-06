#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/sys_config.h>
#include <linux/etherdevice.h>
#include <linux/pinctrl/pinconf-sunxi.h>

struct sunxi_ril_platdata {
	struct regulator *ril_power;
	int gpio_ril_nstandby;
	int gpio_ril_reset;
	char *ril_power_name;
	int nstandby_state;
	int reset_state;
	struct platform_device *pdev;
};
static struct sunxi_ril_platdata *ril_data;
static DEFINE_MUTEX(sunxi_ril_mutex);

static int sunxi_ril_power(struct sunxi_ril_platdata *data, bool on_off)
{
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	int ret = 0;

	if (on_off) {
		ret = regulator_enable(data->ril_power);
		if (ret < 0) {
			dev_err(dev, "regulator ril_power enable failed\n");
			return ret;
		}

		ret = regulator_get_voltage(data->ril_power);
		if (ret < 0) {
			dev_err(dev, "regulator ril_power get voltage failed\n");
			return ret;
		}
		dev_info(dev, "check ril_power voltage: %d\n", ret);
	} else {
		ret = regulator_disable(data->ril_power);
		if (ret < 0) {
			dev_err(dev, "regulator ril_power disable failed\n");
			return ret;
		}
	}
	return 0;
}

static ssize_t nstandby_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ril_data->nstandby_state);
}

static ssize_t nstandby_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	int err;

	dev_info(dev, "set nstandby_state: %s\n", buf);
	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	if (state > 1)
		return -EINVAL;

	mutex_lock(&sunxi_ril_mutex);
	if (state != ril_data->nstandby_state &&
			gpio_is_valid(ril_data->gpio_ril_nstandby)) {
		err = gpio_direction_output(ril_data->gpio_ril_nstandby, state);
		if (err) {
			dev_err(dev, "set power failed\n");
		} else {
			ril_data->nstandby_state = state;
		}
	}
	mutex_unlock(&sunxi_ril_mutex);

	return count;
}

static DEVICE_ATTR(nstandby_state, S_IWUSR | S_IWGRP | S_IRUGO,
		nstandby_state_show, nstandby_state_store);

static ssize_t reset_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ril_data->reset_state);
}

static ssize_t reset_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	int err;

	dev_info(dev, "set reset_state: %s\n", buf);
	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	if (state > 1)
		return -EINVAL;

	mutex_lock(&sunxi_ril_mutex);
	if (state != ril_data->reset_state &&
			gpio_is_valid(ril_data->gpio_ril_reset)) {
		err = gpio_direction_output(ril_data->gpio_ril_reset, state);
		if (err) {
			dev_err(dev, "set power failed\n");
		} else {
			ril_data->reset_state = state;
		}
	}
	mutex_unlock(&sunxi_ril_mutex);

	return count;
}

static DEVICE_ATTR(reset_state, S_IWUSR | S_IWGRP | S_IRUGO,
		reset_state_show, reset_state_store);


static struct attribute *misc_attributes[] = {
	&dev_attr_nstandby_state.attr,
	&dev_attr_reset_state.attr,
	NULL,
};

static struct attribute_group misc_attribute_group = {
	.name  = "rf-ctrl",
	.attrs = misc_attributes,
};

static struct miscdevice sunxi_ril_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "sunxi-ril",
};

static int sunxi_ril_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct sunxi_ril_platdata *data = NULL;
	struct gpio_config config;
	const char *power = NULL;
	char pin_name[32] = {0};
	int clk_gpio = 0;
	unsigned long pin_config = 0;
	int ret = 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	data->pdev = pdev;
	ril_data = data;

	printk("sunxi-ril probe run ....\n");
	if (of_property_read_string(np, "ril_power", &power)) {
		dev_warn(dev, "Missing ril_power.\n");
	} else {
		data->ril_power_name = devm_kzalloc(dev, 64, GFP_KERNEL);
		if (!data->ril_power_name) {
			return -ENOMEM;
		} else {
			strcpy(data->ril_power_name, power);
			dev_info(dev, "ril_power_name (%s)\n",
				data->ril_power_name);
		}
		data->ril_power = regulator_get(dev, data->ril_power_name);
		if (!IS_ERR(data->ril_power)) {
			ret = sunxi_ril_power(data, 1);
			if (ret) {
				dev_err(dev, "ril power on failed\n");
				return ret;
			} else {
				dev_info(dev, "ril power on success\n");
			}
		}
	}

	data->gpio_ril_nstandby = of_get_named_gpio_flags(np,
		"ril_nstandby", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(data->gpio_ril_nstandby)) {
		dev_err(dev, "get gpio ril_nstandby failed\n");
	} else {
		dev_info(dev, "ril_nstandby gpio=%d  mul-sel=%d  pull=%d  drv_level=%d  data=%d\n",
				config.gpio,
				config.mul_sel,
				config.pull,
				config.drv_level,
				config.data);

		ret = devm_gpio_request(dev, data->gpio_ril_nstandby,
				"ril_nstandby");
		if (ret < 0) {
			dev_err(dev, "can't request ril_nstandby gpio %d\n",
				data->gpio_ril_nstandby);
			return ret;
		}

		ret = gpio_direction_output(data->gpio_ril_nstandby, 0);
		if (ret < 0) {
			dev_err(dev, "can't request output direction ril_nstandby gpio %d\n",
				data->gpio_ril_nstandby);
			return ret;
		} else {
			data->nstandby_state = 0;
		}
	}

	data->gpio_ril_reset = of_get_named_gpio_flags(np,
		"ril_reset", 0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(data->gpio_ril_reset)) {
		dev_err(dev, "get gpio ril_reset failed\n");
	} else {
		dev_info(dev, "ril_reset gpio=%d  mul-sel=%d  pull=%d  drv_level=%d  data=%d\n",
				config.gpio,
				config.mul_sel,
				config.pull,
				config.drv_level,
				config.data);

		ret = devm_gpio_request(dev, data->gpio_ril_reset,
				"ril_reset");
		if (ret < 0) {
			dev_err(dev, "can't request ril_nstandby gpio %d\n",
				data->gpio_ril_reset);
			return ret;
		}

		ret = gpio_direction_output(data->gpio_ril_reset, 0);
		if (ret < 0) {
			dev_err(dev, "can't request output direction ril_reset gpio %d\n",
				data->gpio_ril_reset);
			return ret;
		} else {
			data->reset_state = 0;
		}
	}


	ret = misc_register(&sunxi_ril_dev);
	if (ret) {
		dev_err(dev, "sunxi-ril register driver as misc device error!\n");
		return ret;
	}
	ret = sysfs_create_group(&sunxi_ril_dev.this_device->kobj,
			&misc_attribute_group);
	if (ret) {
		dev_err(dev, "sunxi-ril register sysfs create group failed!\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM
static int sunxi_ril_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (ril_data->nstandby_state &&
			gpio_is_valid(ril_data->gpio_ril_nstandby)) {
		gpio_direction_output(ril_data->gpio_ril_nstandby, 1);
	}
	return 0;
}

static int sunxi_ril_resume(struct platform_device *pdev)
{
	if (ril_data->nstandby_state &&
			gpio_is_valid(ril_data->gpio_ril_nstandby)) {
		gpio_direction_output(ril_data->gpio_ril_nstandby, 1);
	}
	return 0;
}
#endif

static int sunxi_ril_remove(struct platform_device *pdev)
{
	WARN_ON(0 != misc_deregister(&sunxi_ril_dev));
	sysfs_remove_group(&(sunxi_ril_dev.this_device->kobj),
			&misc_attribute_group);

	if (!IS_ERR(ril_data->ril_power)) {
		sunxi_ril_power(ril_data, 0);
		regulator_put(ril_data->ril_power);
	}

	return 0;
}

static const struct of_device_id sunxi_ril_ids[] = {
	{ .compatible = "allwinner,sunxi-ril" },
	{ /* Sentinel */ }
};

static struct platform_driver sunxi_ril_driver = {
	.probe	= sunxi_ril_probe,
	.remove	= sunxi_ril_remove,
#ifdef CONFIG_PM
	.suspend = sunxi_ril_suspend,
	.resume = sunxi_ril_resume,
#endif
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-ril",
		.of_match_table	= sunxi_ril_ids,
	},
};

module_platform_driver(sunxi_ril_driver);

MODULE_DESCRIPTION("sunxi ril driver");
MODULE_LICENSE(GPL);
