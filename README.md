# QMC5883L
QMC5883L Linux Kernel Device Driver

# Usage

First, load the driver using following commands:

```
	insmod qmc5883_core.ko
	insmod qmc5883_i2c.ko
```

Next, there should be entries under /sys/bus/iio/devices/iio:device0 folder.
You can use these sysfs entries to control your driver, read data and configure QMC5883L using it.

# Testing

To ensure the driver is working, read raw data from the sysfs entries
