#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>

static struct device *hwmon_dev;

static struct device *ec_device;

// --- fanN_input ---
// Read the current fan speed from the EC's memory
static ssize_t ec_get_fan_speed(u8 idx, u16 *val)
{
	struct cros_ec_device *ec;

	if (!ec_device)
		return -ENODEV;

	ec = dev_get_drvdata(ec_device);

	u8 offset = EC_MEMMAP_FAN + 2 * idx;

	return ec->cmd_readmem(ec, offset, sizeof(*val), val);
}

static ssize_t show_fan_speed(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);

	u16 val;
	if (ec_get_fan_speed(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	// Format as string for sysfs
	return sprintf(buf, "%d\n", val);
}

// --- fanN_target ---
static ssize_t ec_set_target_rpm(u8 idx, u32 *val)
{
	struct {
		struct cros_ec_command msg;
		struct ec_params_pwm_set_fan_target_rpm_v1 p;
	} __packed buf;

	struct cros_ec_device *ec;
	int ret;
	if (!ec_device)
		return -ENODEV;

	ec = dev_get_drvdata(ec_device);

	// Zero out the data
	memset(&buf, 0, sizeof(buf));

	// Set the values
	buf.msg.version = 1;
	buf.msg.command = EC_CMD_PWM_SET_FAN_TARGET_RPM;
	buf.msg.outsize = sizeof(buf.p);
	buf.msg.insize = 0;

	buf.p.rpm = *val;
	buf.p.fan_idx = idx;

	ret = cros_ec_cmd_xfer_status(ec, &buf.msg);
	if (ret < 0) {
		return -EIO;
	}

	return 0;
}

static ssize_t set_fan_target(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);
	u32 val;

	int err;
	err = kstrtou32(buf, 10, &val);
	if (err < 0)
		return err;

	if (ec_set_target_rpm(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	return count;
}

// --- pwmN_enable ---
static ssize_t ec_set_auto_fan_ctrl(u8 idx)
{
	struct {
		struct cros_ec_command msg;
		struct ec_params_auto_fan_ctrl_v1 p;
	} __packed buf;

	struct cros_ec_device *ec;
	int ret;
	if (!ec_device)
		return -ENODEV;

	ec = dev_get_drvdata(ec_device);

	// Zero out the data
	memset(&buf, 0, sizeof(buf));

	// Set the values
	buf.msg.version = 1;
	buf.msg.command = EC_CMD_THERMAL_AUTO_FAN_CTRL;
	buf.msg.outsize = sizeof(buf.p);
	buf.msg.insize = 0;

	buf.p.fan_idx = idx;

	ret = cros_ec_cmd_xfer_status(ec, &buf.msg);
	if (ret < 0) {
		return -EIO;
	}

	return 0;
}

static ssize_t set_pwm_enable(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);

	// The EC doesn't take any arguments for this command,
	// so we don't need to parse the buffer
	// u32 val;

	// int err;
	// err = kstrtou32(buf, 10, &val);
	// if (err < 0)
	// 	return err;

	if (ec_set_auto_fan_ctrl(sen_attr->index) < 0) {
		return -EIO;
	}

	return count;
}

// --- pwmN ---
static ssize_t ec_set_fan_duty(u8 idx, u32 *val)
{
	struct {
		struct cros_ec_command msg;
		struct ec_params_pwm_set_fan_duty_v1 p;
	} __packed buf;

	struct cros_ec_device *ec;
	int ret;
	if (!ec_device)
		return -ENODEV;

	ec = dev_get_drvdata(ec_device);

	// Zero out the data
	memset(&buf, 0, sizeof(buf));

	// Set the values
	buf.msg.version = 1;
	buf.msg.command = EC_CMD_PWM_SET_FAN_DUTY;
	buf.msg.outsize = sizeof(buf.p);
	buf.msg.insize = 0;

	buf.p.percent = *val;
	buf.p.fan_idx = idx;

	ret = cros_ec_cmd_xfer_status(ec, &buf.msg);
	if (ret < 0) {
		return -EIO;
	}

	return 0;
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);
	u32 val;

	int err;
	err = kstrtou32(buf, 10, &val);
	if (err < 0)
		return err;

	if (ec_set_fan_duty(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	return count;
}

#define FW_HWMON_RO_PERMS 0444
#define FW_HWMON_WO_PERMS 0200
#define FW_HWMON_RW_PERMS 0644

// clang-format off
static SENSOR_DEVICE_ATTR(fan1_input, FW_HWMON_RO_PERMS, show_fan_speed, NULL, 0); // Fan Reading
static SENSOR_DEVICE_ATTR(fan1_target, FW_HWMON_WO_PERMS, NULL, set_fan_target, 0); // Target RPM
static SENSOR_DEVICE_ATTR(pwm1_enable, FW_HWMON_WO_PERMS, NULL, set_pwm_enable, 0); // Set Fan Control Mode
static SENSOR_DEVICE_ATTR(pwm1, FW_HWMON_WO_PERMS, NULL, set_pwm, 0); // Set Fan Speed

static SENSOR_DEVICE_ATTR(fan2_input, FW_HWMON_RO_PERMS, show_fan_speed, NULL, 1);
static SENSOR_DEVICE_ATTR(fan2_target, FW_HWMON_WO_PERMS, NULL, set_fan_target, 1);
static SENSOR_DEVICE_ATTR(pwm2_enable, FW_HWMON_WO_PERMS, NULL, set_pwm_enable, 1);
static SENSOR_DEVICE_ATTR(pwm2, FW_HWMON_WO_PERMS, NULL, set_pwm, 1);

static SENSOR_DEVICE_ATTR(fan3_input, FW_HWMON_RO_PERMS, show_fan_speed, NULL, 2);
static SENSOR_DEVICE_ATTR(fan3_target, FW_HWMON_WO_PERMS, NULL, set_fan_target, 2);
static SENSOR_DEVICE_ATTR(pwm3_enable, FW_HWMON_WO_PERMS, NULL, set_pwm_enable, 2);
static SENSOR_DEVICE_ATTR(pwm3, FW_HWMON_WO_PERMS, NULL, set_pwm, 2);

static SENSOR_DEVICE_ATTR(fan4_input, FW_HWMON_RO_PERMS, show_fan_speed, NULL, 3);
static SENSOR_DEVICE_ATTR(fan4_target, FW_HWMON_WO_PERMS, NULL, set_fan_target, 3);
static SENSOR_DEVICE_ATTR(pwm4_enable, FW_HWMON_WO_PERMS, NULL, set_pwm_enable, 3);
static SENSOR_DEVICE_ATTR(pwm4, FW_HWMON_WO_PERMS, NULL, set_pwm, 3);
// clang-format on

static struct attribute *fw_hwmon_attrs[(EC_FAN_SPEED_ENTRIES * 4) + 1] = {
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_target.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,

	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_target.dev_attr.attr,
	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,

	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_target.dev_attr.attr,
	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,

	&sensor_dev_attr_fan4_input.dev_attr.attr,
	&sensor_dev_attr_fan4_target.dev_attr.attr,
	&sensor_dev_attr_pwm4_enable.dev_attr.attr,
	&sensor_dev_attr_pwm4.dev_attr.attr,

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
		ec_get_fan_speed(i, &val);
		// EC returns -1 when the fan is not present
		if (val == -1) {
			// Remove the fan from the list, *4 because we have 4 attributes per fan
			fw_hwmon_attrs[i * 4] = NULL;
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
