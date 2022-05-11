/*
 * I2C interface driver for the QMC5883 chip for low field magnetic sensing.
 *
 * Copyright (C) 2022 GiraffAI
 *
 * Author: Amarnath Revanna <amarnath.revanna@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "qmc5883.h"

enum chips { qmc5883 };

static const struct regmap_range qmc5883_readable_ranges[] = {
	regmap_reg_range(0, QMC5883_CHIP_ID_REG),
};

static const struct regmap_range qmc5883_writable_ranges[] = {
	regmap_reg_range(QMC5883_CONTROL_REG_1, QMC5883_PERIOD_REG),
};

static const struct regmap_range qmc5883_volatile_ranges[] = {
	regmap_reg_range(QMC5883_DATA_OUT_LSB_REGS, QMC5883_TEMP_OUT_REG_HIGH)
};

static const struct regmap_access_table qmc5883_readable_table = {
	.yes_ranges = qmc5883_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(qmc5883_readable_ranges),
};

static const struct regmap_access_table qmc5883_writable_table = {
	.yes_ranges = qmc5883_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(qmc5883_writable_ranges),
};

static const struct regmap_access_table qmc5883_volatile_table = {
	.yes_ranges = qmc5883_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(qmc5883_volatile_ranges),
};

static const struct regmap_config qmc5883_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.rd_table = &qmc5883_readable_table,
	.wr_table = &qmc5883_writable_table,
	.volatile_table = &qmc5883_volatile_table,

	.cache_type = REGCACHE_RBTREE,
};

static int qmc5883_i2c_probe(struct i2c_client *cli,
				const struct i2c_device_id *id)
{
	struct regmap *regmap = devm_regmap_init_i2c(cli,
			&qmc5883_i2c_regmap_config);

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);	
	
	pr_info("Amar -------------> qmc5883_i2c_probe++\n");

	return qmc5883_common_probe(&cli->dev,
			regmap,
			id->driver_data, id->name);
}

/*
static int qmc5883_i2c_probe_new(struct i2c_client *cli)
{
	pr_info("Amar ============> qmc5883_i2c_probe_new++\n");

	return 0;
}
*/

static int qmc5883_i2c_remove(struct i2c_client *cli)
{
	pr_info("Amar: qmc5883_i2c_remove--\n");

	return 0;
}

static const struct of_device_id qmc5883_of_match[] = {
	{ 
		.compatible = "qst,qmc5883", 
		.data = (void *)qmc5883
	},
	{},
};

MODULE_DEVICE_TABLE(of, qmc5883_of_match);


static struct i2c_device_id qmc5883_idtable[] = {
	{ "qmc5883", qmc5883 },
	{},
};

MODULE_DEVICE_TABLE(i2c, qmc5883_idtable);


static struct i2c_driver qmc5883_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "qmc5883",
		.pm = QMC5883_PM_OPS,
		.of_match_table = of_match_ptr(qmc5883_of_match),
	},
	.probe = qmc5883_i2c_probe,
	//.probe_new = qmc5883_i2c_probe_new,
	.remove = qmc5883_i2c_remove,
	.id_table = qmc5883_idtable,
};

module_i2c_driver(qmc5883_driver);


MODULE_AUTHOR("Amarnath Revanna");
MODULE_DESCRIPTION("QMC5883 i2c driver");
MODULE_LICENSE("GPL");

