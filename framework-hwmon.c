#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>

static struct device *hwmon_dev;

static struct device *ec_device;

static ssize_t read_fan_speeds(u8 idx, u16 *val)
{
	struct cros_ec_device *ec;

	if (!ec_device)
		return -ENODEV;

	ec = dev_get_drvdata(ec_device);

	u8 offset = EC_MEMMAP_FAN + 2 * idx;

	return ec->cmd_readmem(ec, offset, sizeof(*val), val);
}

static ssize_t show_fan_speed(struct device *dev,
			      struct device_attribute *dev_attr, char *buf)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(dev_attr);

	u16 val;
	if (read_fan_speeds(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	// Format as string for sysfs
	return sprintf(buf, "%d\n", val);
}

// clang-format off
static SENSOR_DEVICE_ATTR(fan1_input, 0444, show_fan_speed, NULL, 0); // Fan Reading
// static SENSOR_DEVICE_ATTR(fan1_target, 0644, show_fan_target, set_fan_target, 0); // Target RPM
// static SENSOR_DEVICE_ATTR(pwm1_enable, 0644, show_pwm_enable, set_pwm_enable, 0); // Set Fan Control Mode
// static SENSOR_DEVICE_ATTR(pwm1, 0644, show_pwm, set_pwm, 0); // Set Fan Speed

static SENSOR_DEVICE_ATTR(fan2_input, 0444, show_fan_speed, NULL, 1);
static SENSOR_DEVICE_ATTR(fan3_input, 0444, show_fan_speed, NULL, 2);
static SENSOR_DEVICE_ATTR(fan4_input, 0444, show_fan_speed, NULL, 3);
// clang-format on

static struct attribute *fw_hwmon_attrs[EC_FAN_SPEED_ENTRIES + 1] = {
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan4_input.dev_attr.attr,
	NULL,
};

static const struct attribute_group fw_hwmon_group = {
	.attrs = fw_hwmon_attrs,
};

static const struct attribute_group *fw_hwmon_groups[] = { &fw_hwmon_group,
							   NULL };

static int device_match_cros_ec(struct device *dev, const void *foo)
{
	const char *name = dev_name(dev);
	if (strncmp(name, "cros-ec-dev", 11))
		return 0;
	return 1;
}

static int __init fw_hwmon_init(void)
{
    struct cros_ec_device *ec;

	ec_device = bus_find_device(&platform_bus_type, NULL, NULL,
				    device_match_cros_ec);
	if (!ec_device) {
		printk(KERN_WARNING "framework-hwmon: failed to find EC.\n");
		return -EINVAL;
	}
	ec_device = ec_device->parent;
	ec = dev_get_drvdata(ec_device);

	if (!ec->cmd_readmem) {
		printk(KERN_WARNING "framework-hwmon: EC not supported.\n");
		return -EINVAL;
	}

	// Count the number of fans
	for (size_t i = 0; i < EC_FAN_SPEED_ENTRIES; i++) {
		s16 val;
		read_fan_speeds(i, &val);
		// EC returns -1 when the fan is not present
		if (val == -1) {
			// Remove the fan from the list
			fw_hwmon_attrs[i] = NULL;
			// the NULL terminates, so we can stop here
			break;
		}
	}

	hwmon_dev = hwmon_device_register_with_groups(NULL, "framework_hwmon",
						      NULL, fw_hwmon_groups);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	return 0;
}

static void __exit fw_hwmon_exit(void)
{
	hwmon_device_unregister(hwmon_dev);
}

module_init(fw_hwmon_init);
module_exit(fw_hwmon_exit);

MODULE_DESCRIPTION("Framework Laptop HWMON Driver");
MODULE_AUTHOR("Stephen Horvath <stephen@horvath.au>");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: cros_ec");
