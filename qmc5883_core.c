#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>

#include "qmc5883.h"

/*
 * QMC5883: Minimum data output rate
 */
#define QMC5883_RATE_OFFSET			0x02
#define QMC5883_RATE_DEFAULT			0x00
#define QMC5883_RATE_MASK			0x0C

static const char *const qmc5883_meas_conf_modes[] = {"normal", "positivebias",
							"negativebias"};

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


/* Describe chip varints */
struct qmc5883_chip_info {
	const struct iio_chan_spec *channels;
	const int (*regval_to_samp_freq)[2];
	const int n_regval_to_samp_freq;
	//const int *regval_to_nanoscale;
	//const int n_regval_to_nanoscale;
};


static ssize_t qmc5883_show_samp_freq_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qmc5883_data *data = iio_priv(dev_to_iio_dev(dev));
	size_t len = 0;
	int i;

	pr_info("qmc5883_show_samp_freq_avaol++\n");

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

	mutex_lock(&data->lock);
	ret = regmap_update_bits(data->regmap, QMC5883_CONTROL_REG_1,
				QMC5883_RATE_MASK,
				rate << QMC5883_RATE_OFFSET);
	mutex_unlock(&data->lock);

	return ret;
}

static int qmc5883_get_samp_freq_index(struct qmc5883_data *data,
					int val, int val2)
{
	int i;

	for (i = 0; i < data->variant->n_regval_to_samp_freq; i++)
		if (val == data->variant->regval_to_samp_freq[0][0] &&
		val2 == data->variant->regval_to_samp_freq[i][1])
			return i;

	return -EINVAL;
}



static int qmc5883_set_meas_conf(struct qmc5883_data *data, u8 meas_conf)
{
	//AMAR: TODO
	return 0;
}

static 
int qmc5883_show_measurement_configuration(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan)
{
	//AMAR: TODO
	return 0;
}


static 
int qmc5883_set_measurement_configuration(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					unsigned int meas_conf)
{
	struct qmc5883_data *data = iio_priv(indio_dev);

	return qmc5883_set_meas_conf(data, meas_conf);
}

static const struct iio_mount_matrix *
qmc5883_get_mount_matrix(const struct iio_dev *indio_dev,
			const struct iio_chan_spec *chan)
{
	struct qmc5883_data *data = iio_priv(indio_dev);

	return &data->orientation;
}

static int qmc5883_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	//struct qmc5883_data *data = iio_priv(indio_dev);
	int ret = 0;

	pr_info("qmc5883_read_raw++\n");
	
	return ret;
}

static int qmc5883_write_raw_get_fmt(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			long mask)
{
	//struct qmc5883_data *data = iio_priv(indio_dev);
	int ret = 0;

	pr_info("qmc5883_write_raw_get_fmt++\n");

	return ret;
}




static int qmc5883_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int val, int val2, long mask)
{
	struct qmc5883_data *data = iio_priv(indio_dev);
	int rate;

	pr_info("qmc5883_write_raw++\n");

	switch(mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			rate = qmc5883_get_samp_freq_index(data, val, val2);
			if (rate < 0)
				return -EINVAL;

		default:
			return -EINVAL;
	}

}

static irqreturn_t qmc5883_trigger_handler(int irq, void *p)
{
	pr_info("qmc5883_trigger_handler++\n");

	return IRQ_HANDLED;
}


static const struct iio_enum qmc5883_meas_conf_enum = {
	.items = qmc5883_meas_conf_modes,
	.num_items = ARRAY_SIZE(qmc5883_meas_conf_modes),
	.get= qmc5883_show_measurement_configuration,
	.set = qmc5883_set_measurement_configuration,

};

static const struct iio_chan_spec_ext_info qmc5883_ext_info[] = {
	IIO_ENUM("meas_conf", IIO_SHARED_BY_TYPE, &qmc5883_meas_conf_enum),
	IIO_ENUM_AVAILABLE("meas_conf", &qmc5883_meas_conf_enum),
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, qmc5883_get_mount_matrix),
	{}
};

#define QMC5883_CHANNEL(axis, idx)					\
	{								\
		.type = IIO_MAGN,					\
		.modified = 1,						\
		.channel2 = IIO_MOD_##axis,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
			BIT(IIO_CHAN_INFO_SAMP_FREQ),			\
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
	//&iio_dev_attr_scale_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL
};

static const struct attribute_group qmc5883_group = {
	.attrs = qmc5883_attributes,
};



static const unsigned long qmc5883_scan_masks[] = {0x7, 0};

static const struct qmc5883_chip_info qmc5883_chip_info_tbl[] = {
	[QMC5883_ID] = {
		.channels = qmc5883_channels,
		.regval_to_samp_freq = qmc5883_regval_to_samp_freq,
		.n_regval_to_samp_freq = ARRAY_SIZE(qmc5883_regval_to_samp_freq),
		//.regval_to_nanoscale = NULL,
		//.n_regval_to_nanoscale = NULL,
	}
};


static const struct iio_info qmc5883_info = {
	.attrs = &qmc5883_group,
	.read_raw = &qmc5883_read_raw,
	.write_raw = &qmc5883_write_raw,
	.write_raw_get_fmt = &qmc5883_write_raw_get_fmt,
};

static int qmc5883_init(struct qmc5883_data *data)
{
	int ret;

	pr_info("qmc5883_init++\n");

	ret = qmc5883_set_samp_freq(data, QMC5883_RATE_DEFAULT);

	return ret;
}

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
	ret = of_iio_read_mount_matrix(dev, "mount-matrix", &data->orientation);
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
	//qmc5883_set_mode(iio_priv(indio_dev), QMC5883_MODE_SLEEP);
	return ret;
}

EXPORT_SYMBOL(qmc5883_common_probe);

void qmc5883_common_remove(struct device *dev)
{
}

EXPORT_SYMBOL(qmc5883_common_remove);


MODULE_AUTHOR("Amarnath Revanna");
MODULE_DESCRIPTION("QMC5883 Core Driver");
MODULE_LICENSE("GPL");
