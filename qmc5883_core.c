/*
 * Device driver core for the QMC5883 chip for low field magnetic sensing.
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
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/delay.h>

#include "qmc5883.h"

/* Device status */
#define QMC5883_DATA_READY			0x1

/* Mode configuration */
#define QMC5883_MODE_STANDBY			0x00
#define QMC5883_MODE_CONTINUOUS			0x01
#define QMC5883_MODE_MASK			0x03

/*
 * QMC5883: Minimum data output rate
 */
#define QMC5883_RATE_OFFSET			0x02
#define QMC5883_RATE_DEFAULT			0x00
#define QMC5883_RATE_MASK			0x0C

#define QMC5883_RANGE_GAIN_OFFSET		0x04
#define QMC5883_RANGE_GAIN_DEFAULT		0x00
#define QMC5883_RANGE_GAIN_MASK			0x30

#define QMC5883_OVERSAMPLING_OFFSET		0x06
#define QMC5883_OVERSAMPLING_DEFAULT		0x00
#define QMC5883_OVERSAMPLING_MASK		0xC0

/* 
 * From datasheet:
 * Value		/ QMC5883
 * 			/ Data output rate (Hz)
 * 0			/ 10
 * 1			/ 50
 * 2			/ 100
 * 3			/ 200
 */

static const int qmc5883_regval_to_samp_freq[][2] = {
	{10, 0}, {50, 0}, {100, 0}, {200, 0}
};


/* 
 * From datasheet:
 * Value		/ QMC5883
 * 			/ Over Sample Ratio
 * 0			/ 512
 * 1			/ 256
 * 2			/ 128
 * 3			/ 64
 */

static const int qmc5883_regval_to_oversampling_ratio[][2] = {
	{512, 0}, {256, 0}, {128, 0}, {64, 0}
};


/* 
 * From datasheet:
 * Value		/ QMC5883
 * 			/ Full Scale
 * 0			/ 2G
 * 1			/ 8G
 */

static const int qmc5883_regval_to_full_scale[] = {
	2, 8
};

/* Describe chip varints */
struct qmc5883_chip_info {
	const struct iio_chan_spec *channels;
	const int (*regval_to_samp_freq)[2];
	const int n_regval_to_samp_freq;
	const int (*regval_to_oversampling_ratio)[2];
	const int n_regval_to_oversampling_ratio;
	const int *regval_to_full_scale;
	const int n_regval_to_full_scale;
};

static s32 qmc5883_set_mode(struct qmc5883_data *data, u8 operating_mode)
{
	int ret = 0;
	int ret2, val;

	//Amar: TODO Remove below regmap_read and debug prints later
	pr_info("qmc5883_set_mode++, oper_mode=%d\n", operating_mode);
	ret2 = regmap_read(data->regmap, QMC5883_CONTROL_REG_1, &val);
	pr_info("qmc5883_set_mode[1]: ctrl_reg1 = 0x%x \n", val);


	mutex_lock(&data->lock);
	ret = regmap_update_bits(data->regmap, QMC5883_CONTROL_REG_1,
				QMC5883_MODE_MASK, operating_mode);
	mutex_unlock(&data->lock);

	//Amar: TODO Remove below regmap_read and debug prints later
	ret2 = regmap_read(data->regmap, QMC5883_CONTROL_REG_1, &val);
	pr_info("qmc5883_set_mode[2]: ctrl_reg1 = 0x%x \n", val);

	return ret;
}

static int qmc5883_wait_measurement(struct qmc5883_data *data)
{
	int tries = 150;
	unsigned int val;
	int ret;

	pr_info("qmc5883_wait_measurement++\n");

	while (tries-- > 0) {
		ret = regmap_read(data->regmap, QMC5883_STATUS_REG, &val);
		if (ret < 0)
			return ret;
		if (val & QMC5883_DATA_READY)
			break;
		msleep(20);
	}

	if (tries < 0) {
		dev_err(data->dev, "data not ready\n");
		return -EIO;
	}

	return 0;
}

static int qmc5883_read_measurement(struct qmc5883_data *data,
				int idx, int *val)
{
	__le16 values[3];
	int ret;

	mutex_lock(&data->lock);
	ret = qmc5883_wait_measurement(data);
	if (ret < 0) {
		mutex_unlock(&data->lock);
		return ret;
	}
	ret = regmap_bulk_read(data->regmap, QMC5883_DATA_OUT_LSB_REGS,
				values, sizeof(values));
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	//Amar: TODO: Remove below debug print code
	pr_info("Amar: read_meas++\n");

	for (ret = 0; ret < 3; ret++)
		pr_info("values[%d] = %d\n", ret, sign_extend32(le16_to_cpu(values[ret]), 15));

	*val = sign_extend32(le16_to_cpu(values[idx]), 15);

	return IIO_VAL_INT;
}

static const struct iio_mount_matrix *
qmc5883_get_mount_matrix(const struct iio_dev *indio_dev,
			const struct iio_chan_spec *chan)
{
	struct qmc5883_data *data = iio_priv(indio_dev);

	return &data->orientation;
}

static const struct iio_chan_spec_ext_info qmc5883_ext_info[] = {
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, qmc5883_get_mount_matrix),
	{ }
};

static ssize_t qmc5883_show_samp_freq_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qmc5883_data *data = iio_priv(dev_to_iio_dev(dev));
	size_t len = 0;
	int i;

	pr_info("qmc5883_show_samp_freq_avail++\n");

	for (i = 0; i < data->variant->n_regval_to_samp_freq; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%d.%d", data->variant->regval_to_samp_freq[i][0],
			data->variant->regval_to_samp_freq[i][1]);

	buf[len - 1] = '\n';

	return len;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(qmc5883_show_samp_freq_avail);

static int qmc5883_set_samp_freq(struct qmc5883_data *data, u8 rate)
{
	int ret;

	pr_info("qmc5883_set_samp_freq++\n");

	mutex_lock(&data->lock);
	ret = regmap_update_bits(data->regmap, QMC5883_CONTROL_REG_1,
				QMC5883_RATE_MASK,
				rate << QMC5883_RATE_OFFSET);
	mutex_unlock(&data->lock);

	return ret;
}

/*
static int qmc5883_set_oversampling_ratio(struct qmc5883_data *data, u8 ratio)
{
	int ret;

	pr_info("qmc5883_set_oversampling_ratio++\n");

	mutex_lock(&data->lock);
	ret = regmap_update_bits(data->regmap, QMC5883_CONTROL_REG_1,
				QMC5883_OVERSAMPLING_MASK,
				ratio << QMC5883_OVERSAMPLING_OFFSET);
	mutex_unlock(&data->lock);

	return ret;
}
*/

static int qmc5883_get_samp_freq_index(struct qmc5883_data *data,
					int val, int val2)
{
	int i;

	for (i = 0; i < data->variant->n_regval_to_samp_freq; i++) {
		pr_info("Amar: get_samp, val = %d, val2=%d\n", val, val2);
		pr_info("Amar: data->var->regval_freq[i][0] = %d\n", data->variant->regval_to_samp_freq[i][0]);
		pr_info("Amar: data->var->regval_freq[i][1] = %d\n", data->variant->regval_to_samp_freq[i][1]);
		if (val == data->variant->regval_to_samp_freq[i][0] &&
		val2 == data->variant->regval_to_samp_freq[i][1])
			return i;
	}

	return -EINVAL;
}

static ssize_t qmc5883_show_oversampling_ratio_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qmc5883_data *data = iio_priv(dev_to_iio_dev(dev));
	size_t len = 0;
	int i;

	pr_info("qmc5883_show_oversampling_ratio_avail++\n");
	pr_info("qmc5883_show_oversampling_ratio_avail: buf = %p\n", buf);
	pr_info("qmc5883_show_oversampling_ratio_avail: buf = %d\n",
			data->variant->n_regval_to_oversampling_ratio);

	for (i = 0; i < data->variant->n_regval_to_oversampling_ratio; i++) {
		pr_info("Loading to buf=%d.%d\n", data->variant->regval_to_oversampling_ratio[i][0], data->variant->regval_to_oversampling_ratio[i][1]);

		len += scnprintf(buf + len, PAGE_SIZE - len,
		"%d.%d ", data->variant->regval_to_oversampling_ratio[i][0],
		data->variant->regval_to_oversampling_ratio[i][1]);
	}

	buf[len - 1] = '\n';

	pr_info("qmc5883_show_oversampling_ratio_avail: ret len= %ld\n", len);
	return len;
}

static IIO_DEVICE_ATTR(oversampling_ratio_available, S_IRUGO,
		qmc5883_show_oversampling_ratio_avail, NULL, 0);

static ssize_t qmc5883_show_scale_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qmc5883_data *data = iio_priv(dev_to_iio_dev(dev));
	size_t len = 0;
	int i;

	pr_info("qmc5883_show_scaleavail++\n");
	pr_info("qmc5883_show_scale_avail: buf = %p\n", buf);
	pr_info("qmc5883_show_scale_avail: buf = %d\n",
			data->variant->n_regval_to_full_scale);

	for (i = 0; i < data->variant->n_regval_to_full_scale; i++) {
		pr_info("Loading to buf=%d\n", data->variant->regval_to_full_scale[i]);

		len += scnprintf(buf + len, PAGE_SIZE - len,
		"%d ", data->variant->regval_to_full_scale[i]);
	}

	buf[len - 1] = '\n';

	pr_info("qmc5883_show_scale_avail: ret len= %ld\n", len);
	return len;
}

static IIO_DEVICE_ATTR(scale_available, S_IRUGO,
		qmc5883_show_scale_avail, NULL, 0);

static int qmc5883_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct qmc5883_data *data = iio_priv(indio_dev);
	unsigned int rval;
	int ret = 0;

	pr_info("qmc5883_read_raw++\n");
	pr_info("qmc5883_read_raw: chan->scan_index = %d\n", chan->scan_index);
	pr_info("qmc5883_read_raw: chan->address = %ld\n", chan->address);

	switch (mask) {
		case IIO_CHAN_INFO_RAW:
			pr_info("qmc5883_read_raw: IIO_CHAN_INFO_RAW\n");
			return qmc5883_read_measurement(data, chan->scan_index, val);
		case IIO_CHAN_INFO_SCALE:
			pr_info("qmc5883_read_raw: IIO_CHAN_INFO_SCALE\n");
			ret = regmap_read(data->regmap, QMC5883_CONTROL_REG_1, &rval);
			if (ret < 0 || ret > 2)
				return ret;
			rval >>= QMC5883_RANGE_GAIN_OFFSET;
			*val = data->variant->regval_to_full_scale[rval];
			return IIO_VAL_INT;
		case IIO_CHAN_INFO_SAMP_FREQ:
			pr_info("qmc5883_read_raw: IIO_CHAN_INFO_SAMP_FREQ\n");
			ret = regmap_read(data->regmap, QMC5883_CONTROL_REG_1, &rval);
			if (ret < 0)
				return ret;
			rval >>= QMC5883_RATE_OFFSET;
			*val = data->variant->regval_to_samp_freq[rval][0];
			*val2 = data->variant->regval_to_samp_freq[rval][1];
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_CHAN_INFO_OVERSAMPLING_RATIO:
			pr_info("qmc5883_read_raw: IIO_CHAN_INFO_OVERSAMPLING_RATIO\n");
			ret = regmap_read(data->regmap, QMC5883_CONTROL_REG_1, &rval);
			if (ret < 0)
				return ret;
			rval >>= QMC5883_OVERSAMPLING_OFFSET;
			*val = data->variant->regval_to_oversampling_ratio[rval][0];
			*val2 = data->variant->regval_to_oversampling_ratio[rval][1];
			return IIO_VAL_INT;
	}
	return ret;
}

static int qmc5883_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	struct qmc5883_data *data = iio_priv(indio_dev);
	int rate;

	pr_info("qmc5883_write_raw++\n");
	
	switch (mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			rate = qmc5883_get_samp_freq_index(data, val, val2);
			if (rate < 0) {
				pr_info("Amar: get_samp rate error\n");
				return -EINVAL;
			}

			return qmc5883_set_samp_freq(data, rate);

		default:
			pr_info("Amar: case default, returning -EINVAL\n");
			return -EINVAL;
	}
}

static int qmc5883_write_raw_get_fmt(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			long mask)
{
	pr_info("qmc5883_write_raw_get_fmt++\n");

	switch(mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_CHAN_INFO_SCALE:
			return IIO_VAL_INT;
		case IIO_CHAN_INFO_OVERSAMPLING_RATIO:
			return IIO_VAL_INT;
		default:
			return -EINVAL;
	}
}

static irqreturn_t qmc5883_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct qmc5883_data *data = iio_priv(indio_dev);
	int ret;

	pr_info("qmc5883_trigger_handler++\n");

	mutex_lock(&data->lock);
	ret = qmc5883_wait_measurement(data);
	if (ret < 0) {
		mutex_unlock(&data->lock);
		goto done;
	}

	ret = regmap_bulk_read(data->regmap, QMC5883_DATA_OUT_LSB_REGS,
			data->scan.chans, sizeof(data->scan.chans));

	mutex_unlock(&data->lock);
	if (ret < 0)
		goto done;
	
	iio_push_to_buffers_with_timestamp(indio_dev, &data->scan,
					iio_get_time_ns(indio_dev));

done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

#define QMC5883_CHANNEL(axis, idx)					\
	{								\
		.type = IIO_MAGN,					\
		.modified = 1,						\
		.channel2 = IIO_MOD_##axis,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
			BIT(IIO_CHAN_INFO_SAMP_FREQ) |			\
	       		BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),		\
		.scan_index = idx,					\
		.scan_type = {						\
			.sign = 's',					\
			.realbits = 16,					\
			.storagebits = 16,				\
			.endianness = IIO_BE				\
		},							\
		.ext_info = qmc5883_ext_info,	\
	}

static const struct iio_chan_spec qmc5883_channels[] = {
	QMC5883_CHANNEL(X, 0),
	QMC5883_CHANNEL(Y, 1),
	QMC5883_CHANNEL(Z, 2),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static struct attribute *qmc5883_attributes[] = {
	&iio_dev_attr_scale_available.dev_attr.attr,
	&iio_dev_attr_oversampling_ratio_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL
};

static const struct attribute_group qmc5883_group = {
	.attrs = qmc5883_attributes,
};


static const struct qmc5883_chip_info qmc5883_chip_info_tbl[] = {
	[QMC5883_ID] = {
		.channels = qmc5883_channels,
		.regval_to_samp_freq = qmc5883_regval_to_samp_freq,
		.n_regval_to_samp_freq = ARRAY_SIZE(qmc5883_regval_to_samp_freq),
		.regval_to_oversampling_ratio = qmc5883_regval_to_oversampling_ratio,
		.n_regval_to_oversampling_ratio = ARRAY_SIZE(qmc5883_regval_to_oversampling_ratio),
		.regval_to_full_scale = qmc5883_regval_to_full_scale,
		.n_regval_to_full_scale = ARRAY_SIZE(qmc5883_regval_to_full_scale),
	}
};


static int qmc5883_init(struct qmc5883_data *data)
{
        int ret;

        pr_info("qmc5883_init++\n");

        ret = qmc5883_set_samp_freq(data, QMC5883_RATE_DEFAULT);
        if (ret < 0)
                return ret;

        ret = regmap_write(data->regmap, QMC5883_CONTROL_REG_2, 0x00);  // ±2G
        if (ret < 0) {
                pr_err("qmc5883_init: regmap_write failed: %d\n", ret);
                return ret;
        }
        pr_info("qmc5883_init: set ctrl_reg2 = 0x00\n");  // New debug print

        return qmc5883_set_mode(data, QMC5883_MODE_CONTINUOUS);
}



static const struct iio_info qmc5883_info = {
	.attrs = &qmc5883_group,
	.read_raw = &qmc5883_read_raw,
	.write_raw = &qmc5883_write_raw,
	.write_raw_get_fmt = &qmc5883_write_raw_get_fmt,
};

static const unsigned long qmc5883_scan_masks[] = {0x7, 0};

int qmc5883_common_suspend(struct device *dev)
{
	return qmc5883_set_mode(iio_priv(dev_get_drvdata(dev)),
					QMC5883_MODE_STANDBY);
}
EXPORT_SYMBOL(qmc5883_common_suspend);

int qmc5883_common_resume(struct device *dev)
{
	return qmc5883_set_mode(iio_priv(dev_get_drvdata(dev)),
					QMC5883_MODE_CONTINUOUS);
}
EXPORT_SYMBOL(qmc5883_common_resume);

int qmc5883_common_probe(struct device *dev, struct regmap *regmap,
			enum qmc5883_ids id, const char *name)
{
	struct qmc5883_data *data;
	struct iio_dev *indio_dev;
	int ret;

	pr_info("qmc5883_common_probe++\n");

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	dev_set_drvdata(dev, indio_dev);

	/* default settings at probe */
	data = iio_priv(indio_dev);
	data->dev = dev;
	data->regmap = regmap;
	data->variant = &qmc5883_chip_info_tbl[id];
	mutex_init(&data->lock);

	//Amar: TODO: Below call changes in latest kernel version
	ret = iio_read_mount_matrix(dev, &data->orientation);
	if (ret)
		return ret;

	indio_dev->name = name;
	indio_dev->info = &qmc5883_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = data->variant->channels;
	indio_dev->num_channels = 4;
	indio_dev->available_scan_masks = qmc5883_scan_masks;

	ret = qmc5883_init(data);
	if (ret < 0)
		return ret;

	ret = iio_triggered_buffer_setup(indio_dev, NULL,
					qmc5883_trigger_handler, NULL);

	if (ret < 0)
		goto buffer_setup_err;

	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto buffer_cleanup;

	pr_info("qmc5883_common_probei-2++\n");
	
	return 0;

buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);

buffer_setup_err:
	qmc5883_set_mode(iio_priv(indio_dev), QMC5883_MODE_STANDBY);
	return ret;
}
EXPORT_SYMBOL(qmc5883_common_probe);

void qmc5883_common_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);

	/* push to standby mode to save power */
	qmc5883_set_mode(iio_priv(indio_dev), QMC5883_MODE_STANDBY);
}
EXPORT_SYMBOL(qmc5883_common_remove);

MODULE_AUTHOR("Amarnath Revanna");
MODULE_DESCRIPTION("QMC5883 Core Driver");
MODULE_LICENSE("GPL");
