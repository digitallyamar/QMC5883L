/*
 * Header file for QMC5883 magnetometer driver
 *
 * Copyright (C) 2022 GiraffAI
 * Author: Amarnath Revanna <amarnath.revanna@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


 #ifndef QMC5883_CORE_H
 #define QMC5883_CORE_H
 
 #include <linux/regmap.h>
 #include <linux/iio/iio.h>
 
 #define QMC5883_DATA_OUT_LSB_REGS	0X00
 #define QMC5883_STATUS_REG		0X06
 #define QMC5883_TEMP_OUT_REG_LOW	0X07
 #define QMC5883_TEMP_OUT_REG_HIGH	0X08
 #define QMC5883_CONTROL_REG_1		0X09
 #define QMC5883_CONTROL_REG_2		0X0A
 #define QMC5883_PERIOD_REG		0X0B
 #define QMC5883_RESERVED_REG		0x0C
 #define QMC5883_CHIP_ID_REG		0x0D
 
 
 enum qmc5883_ids {
     QMC5883_ID,
 };
 
 /**
  * struct qmc5883_data	- device specific data
  * @dev:		actual device
  * @lock:		update and read regmap data
  * regmap:		hardware access register maps
  * @variant:		describe chip variants
  * @scan:		buffer to pack data for passing to
  * 			iio_push_to_buffers_with_timestamp()
  *
  */
 struct qmc5883_data {
	 struct device *dev;
	 struct mutex lock;
	 struct regmap *regmap;
	 const struct qmc5883_chip_info *variant;
	 struct iio_mount_matrix orientation;
	 struct {
		 __le16 chans[3];
		 s64 timestamp __aligned(8);
	 } scan;
 };
 
 int qmc5883_common_probe(struct device *dev, struct regmap *regmap,
			 enum qmc5883_ids id, const char *name);
 void qmc5883_common_remove(struct device *dev);
 
 int qmc5883_common_suspend(struct device *dev);
 int qmc5883_common_resume(struct device *dev);
 
 
 
 #ifdef CONFIG_PM_SLEEP
 static __maybe_unused SIMPLE_DEV_PM_OPS(qmc5883_pm_ops,
					 qmc5883_common_suspend,
					 qmc5883_common_resume);
 #define QMC5883_PM_OPS (&qmc5883_pm_ops)
 #else
 #define QMC5883_PM_OPS	NULL
 #endif
 
 #endif /* QMC5883_CORE_H */
 