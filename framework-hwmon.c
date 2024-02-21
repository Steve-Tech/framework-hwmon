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
	if (!ec_device)
		return -ENODEV;

	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

	u8 offset = EC_MEMMAP_FAN + 2 * idx;

	return ec->cmd_readmem(ec, offset, sizeof(*val), val);
}

static ssize_t fw_fan_speed_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);

	u16 val;
	if (ec_get_fan_speed(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	// Format as string for sysfs
	return sysfs_emit(buf, "%u\n", val);
}

// --- fanN_target ---
static ssize_t ec_set_target_rpm(u8 idx, u32 *val)
{
	int ret;
	if (!ec_device)
		return -ENODEV;

	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

	struct ec_params_pwm_set_fan_target_rpm_v1 params = {
		.rpm = *val,
		.fan_idx = idx,
	};

	ret = cros_ec_cmd(ec, 1, EC_CMD_PWM_SET_FAN_TARGET_RPM, &params,
			  sizeof(params), NULL, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

static ssize_t ec_get_target_rpm(u8 idx, u32 *val)
{
	int ret;
	if (!ec_device)
		return -ENODEV;

	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

	struct ec_response_pwm_get_fan_rpm resp;

	// index isn't supported, it should only return fan 0's target

	ret = cros_ec_cmd(ec, 0, EC_CMD_PWM_GET_FAN_TARGET_RPM, NULL, 0, &resp,
			  sizeof(resp));
	if (ret < 0)
		return -EIO;

	*val = resp.rpm;

	return 0;
}

static ssize_t fw_fan_target_store(struct device *dev,
				   struct device_attribute *attr,
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

static ssize_t fw_fan_target_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sen_attr = to_sensor_dev_attr(attr);

	// Only fan 0 is supported
	if (sen_attr->index != 0) {
		return -EINVAL;
	}

	u32 val;
	if (ec_get_target_rpm(sen_attr->index, &val) < 0) {
		return -EIO;
	}

	// Format as string for sysfs
	return sysfs_emit(buf, "%u\n", val);
}

// --- pwmN_enable ---
static ssize_t ec_set_auto_fan_ctrl(u8 idx)
{
	int ret;
	if (!ec_device)
		return -ENODEV;

	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

	struct ec_params_auto_fan_ctrl_v1 params = {
		.fan_idx = idx,
	};

	ret = cros_ec_cmd(ec, 1, EC_CMD_THERMAL_AUTO_FAN_CTRL, &params,
			  sizeof(params), NULL, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

static ssize_t fw_pwm_enable_store(struct device *dev,
				   struct device_attribute *attr,
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
	int ret;
	if (!ec_device)
		return -ENODEV;

	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

	struct ec_params_pwm_set_fan_duty_v1 params = {
		.percent = *val,
		.fan_idx = idx,
	};

	ret = cros_ec_cmd(ec, 1, EC_CMD_PWM_SET_FAN_DUTY, &params,
			  sizeof(params), NULL, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

static ssize_t fw_pwm_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t fw_pwm_min_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%i\n", 0);
}

static ssize_t fw_pwm_max_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%i\n", 100);
}

#define FW_ATTRS_PER_FAN 6

// clang-format off
static SENSOR_DEVICE_ATTR_RO(fan1_input, fw_fan_speed, 0); // Fan Reading
static SENSOR_DEVICE_ATTR_RW(fan1_target, fw_fan_target, 0); // Target RPM (RW on fan 0 only)
static SENSOR_DEVICE_ATTR_WO(pwm1_enable, fw_pwm_enable, 0); // Set Fan Control Mode
static SENSOR_DEVICE_ATTR_WO(pwm1, fw_pwm, 0); // Set Fan Speed
static SENSOR_DEVICE_ATTR_RO(pwm1_min, fw_pwm_min, 0); // Min Fan Speed
static SENSOR_DEVICE_ATTR_RO(pwm1_max, fw_pwm_max, 0); // Max Fan Speed

static SENSOR_DEVICE_ATTR_RO(fan2_input, fw_fan_speed, 1);
static SENSOR_DEVICE_ATTR_WO(fan2_target, fw_fan_target, 1);
static SENSOR_DEVICE_ATTR_WO(pwm2_enable, fw_pwm_enable, 1);
static SENSOR_DEVICE_ATTR_WO(pwm2, fw_pwm, 1);
static SENSOR_DEVICE_ATTR_RO(pwm2_min, fw_pwm_min, 1);
static SENSOR_DEVICE_ATTR_RO(pwm2_max, fw_pwm_max, 1);

static SENSOR_DEVICE_ATTR_RO(fan3_input, fw_fan_speed, 2);
static SENSOR_DEVICE_ATTR_WO(fan3_target, fw_fan_target, 2);
static SENSOR_DEVICE_ATTR_WO(pwm3_enable, fw_pwm_enable, 2);
static SENSOR_DEVICE_ATTR_WO(pwm3, fw_pwm, 2);
static SENSOR_DEVICE_ATTR_RO(pwm3_min, fw_pwm_min, 2);
static SENSOR_DEVICE_ATTR_RO(pwm3_max, fw_pwm_max, 2);

static SENSOR_DEVICE_ATTR_RO(fan4_input, fw_fan_speed, 3);
static SENSOR_DEVICE_ATTR_WO(fan4_target, fw_fan_target, 3);
static SENSOR_DEVICE_ATTR_WO(pwm4_enable, fw_pwm_enable, 3);
static SENSOR_DEVICE_ATTR_WO(pwm4, fw_pwm, 3);
static SENSOR_DEVICE_ATTR_RO(pwm4_min, fw_pwm_min, 3);
static SENSOR_DEVICE_ATTR_RO(pwm4_max, fw_pwm_max, 3);
// clang-format on

static struct attribute
	*fw_hwmon_attrs[(EC_FAN_SPEED_ENTRIES * FW_ATTRS_PER_FAN) + 1] = {
		&sensor_dev_attr_fan1_input.dev_attr.attr,
		&sensor_dev_attr_fan1_target.dev_attr.attr,
		&sensor_dev_attr_pwm1_enable.dev_attr.attr,
		&sensor_dev_attr_pwm1.dev_attr.attr,
		&sensor_dev_attr_pwm1_min.dev_attr.attr,
		&sensor_dev_attr_pwm1_max.dev_attr.attr,

		&sensor_dev_attr_fan2_input.dev_attr.attr,
		&sensor_dev_attr_fan2_target.dev_attr.attr,
		&sensor_dev_attr_pwm2_enable.dev_attr.attr,
		&sensor_dev_attr_pwm2.dev_attr.attr,
		&sensor_dev_attr_pwm2_min.dev_attr.attr,
		&sensor_dev_attr_pwm2_max.dev_attr.attr,

		&sensor_dev_attr_fan3_input.dev_attr.attr,
		&sensor_dev_attr_fan3_target.dev_attr.attr,
		&sensor_dev_attr_pwm3_enable.dev_attr.attr,
		&sensor_dev_attr_pwm3.dev_attr.attr,
		&sensor_dev_attr_pwm3_min.dev_attr.attr,
		&sensor_dev_attr_pwm3_max.dev_attr.attr,

		&sensor_dev_attr_fan4_input.dev_attr.attr,
		&sensor_dev_attr_fan4_target.dev_attr.attr,
		&sensor_dev_attr_pwm4_enable.dev_attr.attr,
		&sensor_dev_attr_pwm4.dev_attr.attr,
		&sensor_dev_attr_pwm4_min.dev_attr.attr,
		&sensor_dev_attr_pwm4_max.dev_attr.attr,

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
	ec_device = bus_find_device(&platform_bus_type, NULL, NULL,
				    device_match_cros_ec);
	if (!ec_device) {
		printk(KERN_WARNING "framework-hwmon: failed to find EC.\n");
		return -EINVAL;
	}
	ec_device = ec_device->parent;
	struct cros_ec_device *ec = dev_get_drvdata(ec_device);

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
			// Remove the fan from the list
			fw_hwmon_attrs[i * FW_ATTRS_PER_FAN] = NULL;
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
