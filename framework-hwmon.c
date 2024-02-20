#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/platform_device.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_data/cros_ec_commands.h>

static struct device *hwmon_dev;

static struct device *ec_device;

static ssize_t read_fan_speeds(u8 idx, u16* val) {
	struct cros_ec_device *ec;

	ec = dev_get_drvdata(ec_device);

    u8 offset = EC_MEMMAP_FAN + 2 * idx;

	return ec->cmd_readmem(ec, offset, sizeof(*val), val);
}

static ssize_t show_fan_speed(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
	u16 val;

    u8 idx;

    // fanN_input, the 3rd index is the fan number
    switch (attr->attr.name[3]) {
        case '1':
            idx = 0;
            break;
        case '2':
            idx = 1;
            break;
        case '3':
            idx = 2;
            break;
        case '4':
            idx = 3;
            break;
        default:
            return -EINVAL;
    }

    if (read_fan_speeds(idx, &val) < 0) {
        return -EIO;
    }
    
    return sprintf(buf, "%d\n", val);
}

static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_fan_speed, NULL, 0);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, show_fan_speed, NULL, 0);
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO, show_fan_speed, NULL, 0);
static SENSOR_DEVICE_ATTR(fan4_input, S_IRUGO, show_fan_speed, NULL, 0);

static struct attribute *fw_hwmon_attrs[] = {
    &sensor_dev_attr_fan1_input.dev_attr.attr,
    &sensor_dev_attr_fan2_input.dev_attr.attr,
    &sensor_dev_attr_fan3_input.dev_attr.attr,
    &sensor_dev_attr_fan4_input.dev_attr.attr,
    NULL,
};

static const struct attribute_group fw_hwmon_group = {
    .attrs = fw_hwmon_attrs,
};

static const struct attribute_group *fw_hwmon_groups[] = {
    &fw_hwmon_group,
    NULL
};

static int device_match_cros_ec(struct device *dev, const void* foo) {
	const char* name = dev_name(dev);
	if (strncmp(name, "cros-ec-dev", 11))
		return 0;
	return 1;
}

static int __init fw_hwmon_init(void)
{
	ec_device = bus_find_device(&platform_bus_type, NULL, NULL, device_match_cros_ec);
	if (!ec_device) {
		printk(KERN_WARNING "framework-hwmon: failed to find EC.\n");
		return -EINVAL;
	}
	ec_device = ec_device->parent;

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

    hwmon_dev = hwmon_device_register_with_groups(NULL, "framework_hwmon", NULL, fw_hwmon_groups);
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
