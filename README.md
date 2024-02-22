# Framework-HWMON

A Linux HWMON driver for Framework Laptops, may also work on other devices that use Chromium ECs.

This was mostly just a proof of concept and a learning experience for me, so good luck if you want to use it.

It currently it will read and control the speed of up to 4 fans, but only the first fan has actually been tested.

As I have no idea what I'm doing, a lot of this code is based on the existing [framework-laptop-kmod](https://github.com/DHowett/framework-laptop-kmod) driver, I will hopefully be able to move my code to that project at some point.

## HWMON Interfaces

This driver creates a HWMON interface with the name `framework_hwmon`.

- `fan[1-4]_input` - Read fan speed in RPM (read-only)
- `fan[1-4]_target` - Set target fan speed in RPM
  - read-write on the first fan, write-only on the others
- `fan[1-4]_fault` - Fan removed indicator (read-only)
- `fan[1-4]_alarm` - Fan stall indicator (read-only)
- `pwm[1-4]` - Fan speed control in percent 0-100 (write-only)
- `pwm[1-4]_enable` - Enable automatic fan control (write-only)
  - Currently you can write anything to enable, but I recommend writing `2` in case the driver is updated to support disabling automatic fan control.
  - Writing to the other interfaces will disable automatic fan control.
- `pwm[1-4]_min` - returns 0 (read-only)
- `pwm[1-4]_max` - returns 100 (read-only)
