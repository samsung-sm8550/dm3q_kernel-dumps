// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/math64.h>

/* RTC Register offsets from RTC CTRL REG */
#define PM8XXX_ALARM_CTRL_OFFSET	0x01
#define PM8XXX_RTC_WRITE_OFFSET		0x02
#define PM8XXX_RTC_READ_OFFSET		0x06
#define PM8XXX_ALARM_RW_OFFSET		0x0A

/* RTC_CTRL register bit fields */
#define PM8xxx_RTC_ENABLE		BIT(7)
#define PM8xxx_RTC_ALARM_CLEAR		BIT(0)
#define PM8xxx_RTC_ALARM_ENABLE		BIT(7)

#define NUM_8_BIT_RTC_REGS		0x4

#define RTC_SEC_TO_MSEC(s)	((s) * 1000ULL)
#define RTC_SEC_TO_USEC(s)	((s) * 1000000ULL)
#define RTC_MSTICKS_TO_US(ticks) div_u64(((ticks) * 999000), 1023)

/* Values used for conversion to milli-seconds */
#define RTC_MS_TICKS_MAX		1023
#define RTC_MS_TIME_MAX			999

/**
 * struct pm8xxx_rtc_regs - describe RTC registers per PMIC versions
 * @ctrl: base address of control register
 * @write: base address of write register
 * @read: base address of read register
 * @alarm_ctrl: base address of alarm control register
 * @alarm_ctrl2: base address of alarm control2 register
 * @alarm_rw: base address of alarm read-write register
 * @alarm_en: alarm enable mask
 */
struct pm8xxx_rtc_regs {
	unsigned int ctrl;
	unsigned int write;
	unsigned int read;
	unsigned int alarm_ctrl;
	unsigned int alarm_ctrl2;
	unsigned int alarm_rw;
	unsigned int alarm_en;
	unsigned int read_ms;
};

/**
 * struct pm8xxx_rtc -  rtc driver internal structure
 * @rtc:		rtc device for this driver.
 * @regmap:		regmap used to access RTC registers
 * @allow_set_time:	indicates whether writing to the RTC is allowed
 * @rtc_alarm_irq:	rtc alarm irq number.
 * @regs:		rtc registers description.
 * @rtc_dev:		device structure.
 * @ctrl_reg_lock:	spinlock protecting access to ctrl_reg.
 */
struct pm8xxx_rtc {
	struct rtc_device *rtc;
	struct regmap *regmap;
	bool allow_set_time;
	int rtc_alarm_irq;
	const struct pm8xxx_rtc_regs *regs;
	struct device *rtc_dev;
	spinlock_t ctrl_reg_lock;
};

static int pm8xxx_rtc_read_rtc_data(struct pm8xxx_rtc *rtc_dd, unsigned long *rtc_data)
{
	int rc;
	u8 value[NUM_8_BIT_RTC_REGS];
	unsigned int reg;
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

	rc = regmap_bulk_read(rtc_dd->regmap, regs->read, value, sizeof(value));
	if (rc) {
		dev_err(rtc_dd->rtc_dev, "RTC read rtc data register failed\n");
		return rc;
	}

	/*
	 * Read the LSB again and check if there has been a carry over.
	 * If there is, redo the read operation.
	 */
	rc = regmap_read(rtc_dd->regmap, regs->read, &reg);
	if (rc < 0) {
		dev_err(rtc_dd->rtc_dev, "RTC read rtc data register failed\n");
		return rc;
	}

	if (unlikely(reg < value[0])) {
		rc = regmap_bulk_read(rtc_dd->regmap, regs->read,
				      value, sizeof(value));
		if (rc) {
			dev_err(rtc_dd->rtc_dev, "RTC read rtc data register failed\n");
			return rc;
		}
	}

	*rtc_data = value[0] | (value[1] << 8) | (value[2] << 16) |
			((unsigned long)value[3] << 24);

	return 0;
}

static int pm8xxx_rtc_read_alarm_data(struct pm8xxx_rtc *rtc_dd, unsigned long *alarm_data)
{
	int rc;
	u8 value[NUM_8_BIT_RTC_REGS];

	rc = regmap_bulk_read(rtc_dd->regmap, rtc_dd->regs->alarm_rw, value,
			      sizeof(value));
	if (rc) {
		dev_err(rtc_dd->rtc_dev, "RTC read alarm data failed\n");
		return rc;
	}

	*alarm_data = value[0] | (value[1] << 8) | (value[2] << 16) |
			((unsigned long)value[3] << 24);

	return 0;
}

/*
 * Steps to write the RTC registers.
 * 1. Disable alarm if enabled.
 * 2. Disable rtc if enabled.
 * 3. Write 0x00 to LSB.
 * 4. Write Byte[1], Byte[2], Byte[3] then Byte[0].
 * 5. Enable rtc if disabled in step 2.
 * 6. Enable alarm if disabled in step 1.
 */
static int pm8xxx_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	int rc, i;
	unsigned long secs, irq_flags;
	u8 value[NUM_8_BIT_RTC_REGS], alarm_enabled = 0, rtc_disabled = 0;
	unsigned int ctrl_reg, rtc_ctrl_reg;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

	if (!rtc_dd->allow_set_time)
		return -EACCES;

	secs = rtc_tm_to_time64(tm);

	dev_dbg(dev, "Seconds value to be written to RTC = %lu\n", secs);

	for (i = 0; i < NUM_8_BIT_RTC_REGS; i++) {
		value[i] = secs & 0xFF;
		secs >>= 8;
	}

	spin_lock_irqsave(&rtc_dd->ctrl_reg_lock, irq_flags);

	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl, &ctrl_reg);
	if (rc)
		goto rtc_rw_fail;

	if (ctrl_reg & regs->alarm_en) {
		alarm_enabled = 1;
		ctrl_reg &= ~regs->alarm_en;
		rc = regmap_write(rtc_dd->regmap, regs->alarm_ctrl, ctrl_reg);
		if (rc) {
			dev_err(dev, "Write to RTC Alarm control register failed\n");
			goto rtc_rw_fail;
		}
	}

	/* Disable RTC H/w before writing on RTC register */
	rc = regmap_read(rtc_dd->regmap, regs->ctrl, &rtc_ctrl_reg);
	if (rc)
		goto rtc_rw_fail;

	if (rtc_ctrl_reg & PM8xxx_RTC_ENABLE) {
		rtc_disabled = 1;
		rtc_ctrl_reg &= ~PM8xxx_RTC_ENABLE;
		rc = regmap_write(rtc_dd->regmap, regs->ctrl, rtc_ctrl_reg);
		if (rc) {
			dev_err(dev, "Write to RTC control register failed\n");
			goto rtc_rw_fail;
		}
	}

	/* Write 0 to Byte[0] */
	rc = regmap_write(rtc_dd->regmap, regs->write, 0);
	if (rc) {
		dev_err(dev, "Write to RTC write data register failed\n");
		goto rtc_rw_fail;
	}

	/* Write Byte[1], Byte[2], Byte[3] */
	rc = regmap_bulk_write(rtc_dd->regmap, regs->write + 1,
			       &value[1], sizeof(value) - 1);
	if (rc) {
		dev_err(dev, "Write to RTC write data register failed\n");
		goto rtc_rw_fail;
	}

	/* Write Byte[0] */
	rc = regmap_write(rtc_dd->regmap, regs->write, value[0]);
	if (rc) {
		dev_err(dev, "Write to RTC write data register failed\n");
		goto rtc_rw_fail;
	}

	/* Enable RTC H/w after writing on RTC register */
	if (rtc_disabled) {
		rtc_ctrl_reg |= PM8xxx_RTC_ENABLE;
		rc = regmap_write(rtc_dd->regmap, regs->ctrl, rtc_ctrl_reg);
		if (rc) {
			dev_err(dev, "Write to RTC control register failed\n");
			goto rtc_rw_fail;
		}
	}

	if (alarm_enabled) {
		ctrl_reg |= regs->alarm_en;
		rc = regmap_write(rtc_dd->regmap, regs->alarm_ctrl, ctrl_reg);
		if (rc) {
			dev_err(dev, "Write to RTC Alarm control register failed\n");
			goto rtc_rw_fail;
		}
	}

rtc_rw_fail:
	spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);

	return rc;
}

static int pm8xxx_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	unsigned long secs;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);

	rc = pm8xxx_rtc_read_rtc_data(rtc_dd, &secs);
	if (rc) {
		dev_err(dev, "RTC read time failed\n");
		return rc;
	}

	rtc_time64_to_tm(secs, tm);

	dev_dbg(dev, "secs = %lu, h:m:s == %ptRt, y-m-d = %ptRdr\n", secs, tm, tm);

	return 0;
}

static int pm8xxx_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int rc, i;
	u8 value[NUM_8_BIT_RTC_REGS];
	unsigned long secs, irq_flags;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

	secs = rtc_tm_to_time64(&alarm->time);

	for (i = 0; i < NUM_8_BIT_RTC_REGS; i++) {
		value[i] = secs & 0xFF;
		secs >>= 8;
	}

	rc = regmap_update_bits(rtc_dd->regmap, regs->alarm_ctrl,
				regs->alarm_en, 0);
	if (rc)
		return rc;

	spin_lock_irqsave(&rtc_dd->ctrl_reg_lock, irq_flags);

	rc = regmap_bulk_write(rtc_dd->regmap, regs->alarm_rw, value,
			       sizeof(value));
	if (rc) {
		dev_err(dev, "Write to RTC ALARM register failed\n");
		goto rtc_rw_fail;
	}

	if (alarm->enabled) {
		rc = regmap_update_bits(rtc_dd->regmap, regs->alarm_ctrl,
					regs->alarm_en, regs->alarm_en);
		if (rc)
			goto rtc_rw_fail;
	}

	dev_dbg(dev, "Alarm Set for h:m:s=%ptRt, y-m-d=%ptRdr\n",
		&alarm->time, &alarm->time);
rtc_rw_fail:
	spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);
	return rc;
}

static int pm8xxx_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int rc;
	unsigned int ctrl_reg;
	unsigned long secs;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

	rc = pm8xxx_rtc_read_alarm_data(rtc_dd, &secs);
	if (rc) {
		dev_err(dev, "RTC alarm data read failed\n");
		return rc;
	}

	rtc_time64_to_tm(secs, &alarm->time);

	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl, &ctrl_reg);
	if (rc) {
		dev_err(dev, "Read from RTC alarm control register failed\n");
		return rc;
	}
	alarm->enabled = !!(ctrl_reg & PM8xxx_RTC_ALARM_ENABLE);

	dev_dbg(dev, "Alarm set for - h:m:s=%ptRt, y-m-d=%ptRdr\n",
		&alarm->time, &alarm->time);

	return 0;
}

static int pm8xxx_rtc_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	int rc;
	unsigned long irq_flags;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;
	unsigned int ctrl_reg;
	u8 value[NUM_8_BIT_RTC_REGS] = {0};

	spin_lock_irqsave(&rtc_dd->ctrl_reg_lock, irq_flags);

	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl, &ctrl_reg);
	if (rc)
		goto rtc_rw_fail;

	if (enable)
		ctrl_reg |= regs->alarm_en;
	else
		ctrl_reg &= ~regs->alarm_en;

	rc = regmap_write(rtc_dd->regmap, regs->alarm_ctrl, ctrl_reg);
	if (rc) {
		dev_err(dev, "Write to RTC control register failed\n");
		goto rtc_rw_fail;
	}

	/* Clear Alarm register */
	if (!enable) {
		rc = regmap_bulk_write(rtc_dd->regmap, regs->alarm_rw, value,
				       sizeof(value));
		if (rc) {
			dev_err(dev, "Clear RTC ALARM register failed\n");
			goto rtc_rw_fail;
		}
	}

rtc_rw_fail:
	spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);
	return rc;
}

static ssize_t rtc_us_val_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc;
	u8 value[NUM_8_BIT_RTC_REGS], value_ms1[2], value_ms2[2];
	unsigned long long secs = 0, mticks = 0, usecs = 0, rtc_us_total = 0;
	unsigned int reg;
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev->parent);
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

reread:
	rc = regmap_bulk_read(rtc_dd->regmap, regs->read, value, sizeof(value));
	if (rc) {
		dev_err(dev, "RTC read data register failed\n");
		return rc;
	}

	/* Read milli-second value */
	rc = regmap_bulk_read(rtc_dd->regmap, regs->read_ms, value_ms1, sizeof(value_ms1));
	if (rc) {
		dev_err(dev, "RTC read data register failed\n");
		return rc;
	}

	/* Read milli-second value again*/
	rc = regmap_bulk_read(rtc_dd->regmap, regs->read_ms, value_ms2, sizeof(value_ms2));
	if (rc) {
		dev_err(dev, "RTC read data register failed\n");
		return rc;
	}
	/* check for rollover in ms value like from 1023 to 0 */
	if (value_ms1[1] != value_ms2[1])
		goto reread;

	mticks = value_ms2[0] | (value_ms2[1] << 8);
	if (mticks == 1023 || mticks == 0)
		goto reread;
	/*
	 * Read the LSB again and check if there has been a carry over.
	 * If there is, redo the read operation.
	 */
	rc = regmap_read(rtc_dd->regmap, regs->read, &reg);
	if (rc < 0) {
		dev_err(dev, "RTC read data register failed\n");
		return rc;
	}

	if (unlikely(reg < value[0])) {
		rc = regmap_bulk_read(rtc_dd->regmap, regs->read,
				      value, sizeof(value));
		if (rc) {
			dev_err(dev, "RTC read data register failed\n");
			return rc;
		}
	}

	secs = value[0] | (value[1] << 8) | (value[2] << 16) |
	       ((unsigned long long)value[3] << 24);

	/* Mapping 1023 ticks to 999 milli-seconds */
	usecs = RTC_MSTICKS_TO_US(mticks);

	rtc_us_total = RTC_SEC_TO_USEC(secs) + usecs;

	return scnprintf(buf, PAGE_SIZE, "%llu\n", rtc_us_total);
}

static DEVICE_ATTR_RO(rtc_us_val);

static struct attribute *pm8xxx_rtc_attrs[] = {
	&dev_attr_rtc_us_val.attr,
	NULL,
};

static const struct attribute_group pm8xxx_rtc_group = {
	.attrs = pm8xxx_rtc_attrs,
};

static const struct rtc_class_ops pm8xxx_rtc_ops = {
	.read_time	= pm8xxx_rtc_read_time,
	.set_time	= pm8xxx_rtc_set_time,
	.set_alarm	= pm8xxx_rtc_set_alarm,
	.read_alarm	= pm8xxx_rtc_read_alarm,
	.alarm_irq_enable = pm8xxx_rtc_alarm_irq_enable,
};

static irqreturn_t pm8xxx_alarm_trigger(int irq, void *dev_id)
{
	struct pm8xxx_rtc *rtc_dd = dev_id;
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;
	unsigned int ctrl_reg;
	int rc;

	rtc_update_irq(rtc_dd->rtc, 1, RTC_IRQF | RTC_AF);

	spin_lock(&rtc_dd->ctrl_reg_lock);

	/* Clear the alarm enable bit */
	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl, &ctrl_reg);
	if (rc) {
		spin_unlock(&rtc_dd->ctrl_reg_lock);
		goto rtc_alarm_handled;
	}

	ctrl_reg &= ~regs->alarm_en;

	rc = regmap_write(rtc_dd->regmap, regs->alarm_ctrl, ctrl_reg);
	if (rc) {
		spin_unlock(&rtc_dd->ctrl_reg_lock);
		dev_err(rtc_dd->rtc_dev,
			"Write to alarm control register failed\n");
		goto rtc_alarm_handled;
	}

	spin_unlock(&rtc_dd->ctrl_reg_lock);

	/* Clear RTC alarm register */
	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl2, &ctrl_reg);
	if (rc) {
		dev_err(rtc_dd->rtc_dev,
			"RTC Alarm control2 register read failed\n");
		goto rtc_alarm_handled;
	}

	ctrl_reg |= PM8xxx_RTC_ALARM_CLEAR;
	rc = regmap_write(rtc_dd->regmap, regs->alarm_ctrl2, ctrl_reg);
	if (rc)
		dev_err(rtc_dd->rtc_dev,
			"Write to RTC Alarm control2 register failed\n");

rtc_alarm_handled:
	return IRQ_HANDLED;
}

/*
 * Trigger the alarm event and clear the alarm settings
 * if the alarm data has been behind the RTC data which
 * means the alarm has been triggered before the driver
 * is probed.
 */
static int pm8xxx_rtc_init_alarm(struct pm8xxx_rtc *rtc_dd)
{
	int rc;
	unsigned long rtc_data, alarm_data, irq_flags;
	unsigned int ctrl_reg, alarm_en;
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;

	spin_lock_irqsave(&rtc_dd->ctrl_reg_lock, irq_flags);

	rc = pm8xxx_rtc_read_rtc_data(rtc_dd, &rtc_data);
	if (rc) {
		spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);
		dev_err(rtc_dd->rtc_dev, "rtc read rtc data failed\n");
		return rc;
	}

	rc = pm8xxx_rtc_read_alarm_data(rtc_dd, &alarm_data);
	if (rc) {
		spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);
		dev_err(rtc_dd->rtc_dev, "rtc read alarm data failed\n");
		return rc;
	}

	rc = regmap_read(rtc_dd->regmap, regs->alarm_ctrl, &ctrl_reg);
	if (rc) {
		spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);
		dev_err(rtc_dd->rtc_dev, "Read from RTC alarm control register failed\n");
		return rc;
	}

	spin_unlock_irqrestore(&rtc_dd->ctrl_reg_lock, irq_flags);

	alarm_en = !!(ctrl_reg & PM8xxx_RTC_ALARM_ENABLE);

	if (alarm_en && rtc_data >= alarm_data)
		pm8xxx_alarm_trigger(0, rtc_dd);

	return 0;
}

static int pm8xxx_rtc_enable(struct pm8xxx_rtc *rtc_dd)
{
	const struct pm8xxx_rtc_regs *regs = rtc_dd->regs;
	unsigned int ctrl_reg;
	int rc;

	/* Check if the RTC is on, else turn it on */
	rc = regmap_read(rtc_dd->regmap, regs->ctrl, &ctrl_reg);
	if (rc)
		return rc;

	if (!(ctrl_reg & PM8xxx_RTC_ENABLE)) {
		ctrl_reg |= PM8xxx_RTC_ENABLE;
		rc = regmap_write(rtc_dd->regmap, regs->ctrl, ctrl_reg);
		if (rc)
			return rc;
	}

	return 0;
}

static const struct pm8xxx_rtc_regs pm8921_regs = {
	.ctrl		= 0x11d,
	.write		= 0x11f,
	.read		= 0x123,
	.alarm_rw	= 0x127,
	.alarm_ctrl	= 0x11d,
	.alarm_ctrl2	= 0x11e,
	.alarm_en	= BIT(1),
};

static const struct pm8xxx_rtc_regs pm8058_regs = {
	.ctrl		= 0x1e8,
	.write		= 0x1ea,
	.read		= 0x1ee,
	.alarm_rw	= 0x1f2,
	.alarm_ctrl	= 0x1e8,
	.alarm_ctrl2	= 0x1e9,
	.alarm_en	= BIT(1),
};

static const struct pm8xxx_rtc_regs pm8941_regs = {
	.ctrl		= 0x6046,
	.write		= 0x6040,
	.read		= 0x6048,
	.alarm_rw	= 0x6140,
	.alarm_ctrl	= 0x6146,
	.alarm_ctrl2	= 0x6148,
	.alarm_en	= BIT(7),
};

static const struct pm8xxx_rtc_regs pmk8350_regs = {
	.ctrl		= 0x6146,
	.write		= 0x6140,
	.read		= 0x6148,
	.alarm_rw	= 0x6240,
	.alarm_ctrl	= 0x6246,
	.alarm_ctrl2	= 0x6248,
	.alarm_en	= BIT(7),
	.read_ms	= 0x6162,
};

static const struct pm8xxx_rtc_regs pm5100_regs = {
	.ctrl		= 0x6446,
	.write		= 0x6440,
	.read		= 0x6448,
	.alarm_rw	= 0x6540,
	.alarm_ctrl	= 0x6546,
	.alarm_ctrl2	= 0x6548,
	.alarm_en	= BIT(7),
	.read_ms	= 0x6462,
};

/*
 * Hardcoded RTC bases until IORESOURCE_REG mapping is figured out
 */
static const struct of_device_id pm8xxx_id_table[] = {
	{ .compatible = "qcom,pm8921-rtc", .data = &pm8921_regs },
	{ .compatible = "qcom,pm8018-rtc", .data = &pm8921_regs },
	{ .compatible = "qcom,pm8058-rtc", .data = &pm8058_regs },
	{ .compatible = "qcom,pm8941-rtc", .data = &pm8941_regs },
	{ .compatible = "qcom,pmk8350-rtc", .data = &pmk8350_regs },
	{ .compatible = "qcom,pm5100-rtc", .data = &pm5100_regs },
	{ },
};
MODULE_DEVICE_TABLE(of, pm8xxx_id_table);

static int pm8xxx_rtc_probe(struct platform_device *pdev)
{
	int rc;
	struct pm8xxx_rtc *rtc_dd;
	const struct of_device_id *match;

	match = of_match_node(pm8xxx_id_table, pdev->dev.of_node);
	if (!match)
		return -ENXIO;

	rtc_dd = devm_kzalloc(&pdev->dev, sizeof(*rtc_dd), GFP_KERNEL);
	if (rtc_dd == NULL)
		return -ENOMEM;

	/* Initialise spinlock to protect RTC control register */
	spin_lock_init(&rtc_dd->ctrl_reg_lock);

	rtc_dd->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!rtc_dd->regmap) {
		dev_err(&pdev->dev, "Parent regmap unavailable.\n");
		return -ENXIO;
	}

	rtc_dd->rtc_alarm_irq = platform_get_irq(pdev, 0);
	if (rtc_dd->rtc_alarm_irq < 0)
		return -ENXIO;

	rtc_dd->allow_set_time = of_property_read_bool(pdev->dev.of_node,
						      "allow-set-time");

	rtc_dd->regs = match->data;
	rtc_dd->rtc_dev = &pdev->dev;

	rc = pm8xxx_rtc_enable(rtc_dd);
	if (rc)
		return rc;

	platform_set_drvdata(pdev, rtc_dd);

	device_init_wakeup(&pdev->dev, 1);

	/* Register the RTC device */
	rtc_dd->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc_dd->rtc))
		return PTR_ERR(rtc_dd->rtc);

	rtc_dd->rtc->ops = &pm8xxx_rtc_ops;
	rtc_dd->rtc->range_max = U32_MAX;


	rc = rtc_add_group(rtc_dd->rtc, &pm8xxx_rtc_group);
	if (rc)
		return rc;


	/* Request the alarm IRQ */
	rc = devm_request_any_context_irq(&pdev->dev, rtc_dd->rtc_alarm_irq,
					  pm8xxx_alarm_trigger,
					  IRQF_TRIGGER_RISING,
					  "pm8xxx_rtc_alarm", rtc_dd);
	if (rc < 0) {
		dev_err(&pdev->dev, "Request IRQ failed (%d)\n", rc);
		return rc;
	}

	rc =  devm_rtc_register_device(rtc_dd->rtc);
	if (rc < 0) {
		dev_err(&pdev->dev, "Register RTC device failed\n");
		return rc;
	}

	if (of_property_read_bool(pdev->dev.of_node, "qcom,disable-alarm-wakeup"))
		device_set_wakeup_capable(&pdev->dev, false);

	return pm8xxx_rtc_init_alarm(rtc_dd);
}


#if IS_ENABLED(CONFIG_RTC_AUTO_PWRON)
static struct rtc_wkalrm pwron_alarm;

void pmic_rtc_setalarm(struct rtc_wkalrm *alm)
{
	memcpy(&pwron_alarm, alm, sizeof(struct rtc_wkalrm));
}
EXPORT_SYMBOL(pmic_rtc_setalarm);

static void pm8xxx_rtc_shutdown(struct platform_device *pdev)
{
	struct rtc_wkalrm alarm;
	int rc = 0;

	if (pdev) {
		pm8xxx_rtc_set_alarm(&pdev->dev, &pwron_alarm);
		rc = pm8xxx_rtc_read_alarm(&pdev->dev, &alarm);
		if (!rc) {
			pr_info("%s: %d-%02d-%02d %02d:%02d:%02d\n", __func__,
				alarm.time.tm_year + 1900, alarm.time.tm_mon + 1, alarm.time.tm_mday,
				alarm.time.tm_hour, alarm.time.tm_min, alarm.time.tm_sec);
		}
	}
	else
		pr_err("%s: spmi device not found\n", __func__);
}
#else
static void pm8xxx_rtc_shutdown(struct platform_device *pdev)
{
	struct pm8xxx_rtc *rtc_dd = platform_get_drvdata(pdev);

	devm_free_irq(rtc_dd->rtc_dev, rtc_dd->rtc_alarm_irq, rtc_dd);
}
#endif
static int pm8xxx_rtc_restore(struct device *dev)
{
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);
	int rc;

	/* Request the alarm IRQ */
	rc = devm_request_any_context_irq(rtc_dd->rtc_dev,
					  rtc_dd->rtc_alarm_irq,
					  pm8xxx_alarm_trigger,
					  IRQF_TRIGGER_RISING,
					  "pm8xxx_rtc_alarm", rtc_dd);
	if (rc < 0) {
		dev_err(rtc_dd->rtc_dev, "Request IRQ failed (%d)\n", rc);
		return rc;
	}

	return pm8xxx_rtc_enable(rtc_dd);
}

static int pm8xxx_rtc_freeze(struct device *dev)
{
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);

	devm_free_irq(rtc_dd->rtc_dev, rtc_dd->rtc_alarm_irq, rtc_dd);

	return 0;
}

static int pm8xxx_rtc_resume(struct device *dev)
{
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);

#ifdef CONFIG_DEEPSLEEP
	if (pm_suspend_via_firmware())
		return pm8xxx_rtc_restore(dev);
#endif
	if (device_may_wakeup(dev))
		disable_irq_wake(rtc_dd->rtc_alarm_irq);

	return 0;
}

static int pm8xxx_rtc_suspend(struct device *dev)
{
	struct pm8xxx_rtc *rtc_dd = dev_get_drvdata(dev);

#ifdef CONFIG_DEEPSLEEP
	if (pm_suspend_via_firmware())
		return pm8xxx_rtc_freeze(dev);
#endif
	if (device_may_wakeup(dev))
		enable_irq_wake(rtc_dd->rtc_alarm_irq);

	return 0;
}

static const struct dev_pm_ops pm8xxx_rtc_pm_ops = {
	.freeze = pm8xxx_rtc_freeze,
	.restore = pm8xxx_rtc_restore,
	.suspend = pm8xxx_rtc_suspend,
	.resume = pm8xxx_rtc_resume,
};

static struct platform_driver pm8xxx_rtc_driver = {
	.probe		= pm8xxx_rtc_probe,
	.driver	= {
		.name		= "rtc-pm8xxx",
		.pm		= &pm8xxx_rtc_pm_ops,
		.of_match_table	= pm8xxx_id_table,
	},
	.shutdown	= pm8xxx_rtc_shutdown,
};

module_platform_driver(pm8xxx_rtc_driver);

MODULE_ALIAS("platform:rtc-pm8xxx");
MODULE_DESCRIPTION("PMIC8xxx RTC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Anirudh Ghayal <aghayal@codeaurora.org>");
