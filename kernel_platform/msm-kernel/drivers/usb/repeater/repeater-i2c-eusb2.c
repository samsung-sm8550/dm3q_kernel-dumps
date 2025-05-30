// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/qti-regmap-debugfs.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/usb/repeater.h>
#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
#include <linux/sec_class.h>
#include <linux/mutex.h>
#endif

#define EUSB2_3P0_VOL_MIN			3075000 /* uV */
#define EUSB2_3P0_VOL_MAX			3300000 /* uV */
#define EUSB2_3P0_HPM_LOAD			12000	/* uA */

#define EUSB2_1P8_VOL_MIN			1800000 /* uV */
#define EUSB2_1P8_VOL_MAX			1800000 /* uV */
#define EUSB2_1P8_HPM_LOAD			32000	/* uA */

/* NXP eUSB2 repeater registers */
#define RESET_CONTROL			0x01
#define LINK_CONTROL1			0x02
#define LINK_CONTROL2			0x03
#define eUSB2_RX_CONTROL		0x04
#define eUSB2_TX_CONTROL		0x05
#define USB2_RX_CONTROL			0x06
#define USB2_TX_CONTROL1		0x07
#define USB2_TX_CONTROL2		0x08
#define USB2_HS_TERMINATION		0x09
#define USB2_HS_DISCONNECT_THRESHOLD	0x0A
#define RAP_SIGNATURE			0x0D
#define VDX_CONTROL			0x0E
#define DEVICE_STATUS			0x0F
#define LINK_STATUS			0x10
#define REVISION_ID			0x13
#define CHIP_ID_0			0x14
#define CHIP_ID_1			0x15
#define CHIP_ID_2			0x16

/* TI eUSB2 repeater registers */
#define GPIO0_CONFIG			0x00
#define GPIO1_CONFIG			0x40
#define UART_PORT1			0x50
#define EXTRA_PORT1			0x51
#define U_TX_ADJUST_PORT1		0x70
#define U_HS_TX_PRE_EMPHASIS_P1		0x71
#define U_RX_ADJUST_PORT1		0x72
#define U_DISCONNECT_SQUELCH_PORT1	0x73
#define E_HS_TX_PRE_EMPHASIS_P1		0x77
#define E_TX_ADJUST_PORT1		0x78
#define E_RX_ADJUST_PORT1		0x79
#define REV_ID				0xB0
#define GLOBAL_CONFIG			0xB2
#define INT_ENABLE_1			0xB3
#define INT_ENABLE_2			0xB4
#define BC_CONTROL			0xB6
#define BC_STATUS_1			0xB7
#define INT_STATUS_1			0xA3
#define INT_STATUS_2			0xA4

/* Diodes eUSB2 repeater PI3EUSB1100 registers */
#define DIODES_PI3EUSB1100_M_F_CONTROL				0x00
#define DIODES_PI3EUSB1100_USB2_TX_EQ_CONTROL			0x01
#define DIODES_PI3EUSB1100_USB2_TX_EQ_OUT_CURRENT_CONTROL	0x02
#define DIODES_PI3EUSB1100_USB2_RX_EQ_CONTROL			0x03
#define DIODES_PI3EUSB1100_USB2_RX_EQ_SSS_CONTROL		0x04
#define DIODES_PI3EUSB1100_USB2_SDO_CONTROL			0x05
#define DIODES_PI3EUSB1100_USB2_TX_OUT_SWING_CONTROL		0x06
#define DIODES_PI3EUSB1100_USB2_FS_OUT_DDSS_CONTROL		0x07
#define DIODES_PI3EUSB1100_REV_ID				0x14
#define DIODES_PI3EUSB1100_DEV_ID_LO				0x15
#define DIODES_PI3EUSB1100_DEV_ID_HI				0x16

#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
#define ADDRESS_START eUSB2_RX_CONTROL
#define ADDRESS_END USB2_HS_DISCONNECT_THRESHOLD
#define TUNE_BUF_COUNT 20
#define TUNE_BUF_SIZE 25
#define TUNE_MAX_NXP 17
#define TUNE_MAX_TI 19

static u8 tune_map_nxp[TUNE_MAX_NXP] = {
	RESET_CONTROL,
	LINK_CONTROL1,
	LINK_CONTROL2,
	eUSB2_RX_CONTROL,
	eUSB2_TX_CONTROL,
	USB2_RX_CONTROL,
	USB2_TX_CONTROL1,
	USB2_TX_CONTROL2,
	USB2_HS_TERMINATION,
	USB2_HS_DISCONNECT_THRESHOLD,
	RAP_SIGNATURE,
	DEVICE_STATUS,
	LINK_STATUS,
	REVISION_ID,
	CHIP_ID_0,
	CHIP_ID_1,
	CHIP_ID_2,
};

static u8 tune_map_ti[TUNE_MAX_TI] = {
	GPIO0_CONFIG,
	GPIO1_CONFIG,
	UART_PORT1,
	EXTRA_PORT1,
	U_TX_ADJUST_PORT1,
	U_HS_TX_PRE_EMPHASIS_P1,
	U_RX_ADJUST_PORT1,
	U_DISCONNECT_SQUELCH_PORT1,
	E_HS_TX_PRE_EMPHASIS_P1,
	E_TX_ADJUST_PORT1,
	E_RX_ADJUST_PORT1,
	REV_ID,
	GLOBAL_CONFIG,
	INT_ENABLE_1,
	INT_ENABLE_2,
	BC_CONTROL,
	BC_STATUS_1,
	INT_STATUS_1,
	INT_STATUS_2,
};
#endif

enum eusb2_repeater_type {
	TI_REPEATER,
	NXP_REPEATER,
	DIODES_REPEATER_PI3EUSB1100,
};

struct i2c_repeater_chip {
	enum eusb2_repeater_type repeater_type;
};

#define MAX_PROP_SIZE 32

struct repeater_vreg {
	struct regulator *reg;
	int min_uV;
	int max_uV;
	int max_uA;
};

struct eusb2_repeater {
	struct device			*dev;
	struct usb_repeater		ur;
	struct regmap			*regmap;
	const struct i2c_repeater_chip	*chip;
	u16				reg_base;
	struct repeater_vreg		*vdd18;
	struct repeater_vreg		*vdd3;
	bool				power_enabled;

	struct gpio_desc		*reset_gpiod;
	u32				*param_override_seq;
	u8				param_override_seq_cnt;
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	u32				*param_host_override_seq;
	u8				param_host_override_seq_cnt;
#endif
#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	struct mutex	er_tune_lock;
	int				tune_buf_cnt;
	u8				tune_buf[TUNE_BUF_COUNT][2];
	bool			er_tune_init_done;
#endif
};

static const struct regmap_config eusb2_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	struct eusb2_repeater *ter = NULL;
#endif

#undef dev_dbg
#define dev_dbg dev_err

static int eusb2_i2c_read_reg(struct eusb2_repeater *er, u8 reg, u8 *val)
{
	int ret;
	unsigned int reg_val;
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	int i;
#endif

	ret = regmap_read(er->regmap, reg, &reg_val);
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	for (i = 0; i < 3 && ret < 0; i++) {
		dev_err(er->dev, "Failed to read reg:0x%02x ret=%d\n", reg, ret);
		usleep_range(400, 450);
		ret = regmap_read(er->regmap, reg, &reg_val);
	}
#endif
	if (ret < 0) {
		dev_err(er->dev, "Failed to read reg:0x%02x ret=%d\n", reg, ret);
		return ret;
	}

	*val = reg_val;
	dev_dbg(er->dev, "read reg:0x%02x val:0x%02x\n", reg, *val);

	return 0;
}

static int eusb2_i2c_write_reg(struct eusb2_repeater *er, u8 reg, u8 val)
{
	int ret;
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	int i;
#endif

	ret = regmap_write(er->regmap, reg, val);
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	for (i = 0; i < 3 && ret < 0; i++) {
		dev_err(er->dev, "failed to write 0x%02x to reg: 0x%02x ret=%d\n", val, reg, ret);
		usleep_range(400, 450);
		ret = regmap_write(er->regmap, reg, val);
	}
#endif
	if (ret < 0) {
		dev_err(er->dev, "failed to write 0x%02x to reg: 0x%02x ret=%d\n", val, reg, ret);
		return ret;
	}

	dev_dbg(er->dev, "write reg:0x%02x val:0x%02x\n", reg, val);

	return 0;
}

static int repeater_parse_vreg_info(struct device *dev, char *name,
				    struct repeater_vreg **out_vreg)
{
	struct device_node *np = dev->of_node;
	struct repeater_vreg *vreg = NULL;
	char prop_name[MAX_PROP_SIZE];
	int ret = 0;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_err(dev, "Unable to parse the phandle of %s supply\n", name);
		return -ENODEV;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	snprintf(prop_name, MAX_PROP_SIZE, "%s", name);
	vreg->reg = devm_regulator_get(dev, prop_name);
	if (IS_ERR(vreg->reg)) {
		dev_err(dev, "Unable to get %s supply\n", name);
		ret = PTR_ERR(vreg->reg);
		return ret;
	}
	dev_dbg(dev, "get %s supply OK\n", name);

	snprintf(prop_name, MAX_PROP_SIZE, "%s-hpm-load", name);
	ret = of_property_read_u32(np, prop_name, &vreg->max_uA);
	if (ret) {
		if (!strcmp(name, "vdd3")) {
			vreg->max_uA = EUSB2_3P0_HPM_LOAD;
		} else if (!strcmp(name, "vdd18")) {
			vreg->max_uA = EUSB2_1P8_HPM_LOAD;
		} else {
			dev_err(dev, "Failed to parse hpm load for %s supply\n", name);
			return ret;
		}
		dev_info(dev, "Unable to get %s-hpm-load, using default\n", name);
		ret = 0;
	}
	dev_dbg(dev, "get vreg->max_uA %u OK\n", vreg->max_uA);

	snprintf(prop_name, MAX_PROP_SIZE, "%s-vol-min", name);
	ret = of_property_read_u32(np, prop_name, &vreg->min_uV);
	if (ret) {
		if (!strcmp(name, "vdd3")) {
			vreg->min_uV = EUSB2_3P0_VOL_MIN;
		} else if (!strcmp(name, "vdd18")) {
			vreg->min_uV = EUSB2_1P8_VOL_MIN;
		} else {
			dev_err(dev, "Failed to parse min voltage for %s supply\n", name);
			return ret;
		}
		dev_info(dev, "Unable to get %s-min-uV, using default\n", name);
		ret = 0;
	}
	dev_dbg(dev, "get vreg->min_uV %u OK\n", vreg->min_uV);

	snprintf(prop_name, MAX_PROP_SIZE, "%s-vol-max", name);
	ret = of_property_read_u32(np, prop_name, &vreg->max_uV);
	if (ret) {
		if (!strcmp(name, "vdd3")) {
			vreg->max_uV = EUSB2_3P0_VOL_MAX;
		} else if (!strcmp(name, "vdd18")) {
			vreg->max_uV = EUSB2_1P8_VOL_MAX;
		} else {
			dev_err(dev, "Failed to parse max voltage for %s supply\n", name);
			return ret;
		}
		dev_info(dev, "Unable to get %s-max_uV, using default\n", name);
		ret = 0;
	}
	dev_dbg(dev, "get vreg->max_uV %u OK\n", vreg->max_uV);

	*out_vreg = vreg;

	return ret;
}

static int repeater_setup_vreg(struct eusb2_repeater *er)
{
	struct device *dev = er->dev;
	int ret = 0;

	ret = repeater_parse_vreg_info(dev, "vdd3", &er->vdd3);
	if (ret) {
		dev_err(dev, "Failed to parse vdd3 vreg\n");
		return ret;
	}

	ret = repeater_parse_vreg_info(dev, "vdd18", &er->vdd18);
	if (ret) {
		dev_err(dev, "Failed to parse vdd18 vreg\n");
		return ret;
	}

	return ret;
}

static void eusb2_repeater_update_seq(struct eusb2_repeater *er, u32 *seq, u8 cnt)
{
	int i;

	dev_dbg(er->ur.dev, "param override seq count:%d\n", cnt);
	for (i = 0; i < cnt; i = i+2) {
		dev_dbg(er->ur.dev, "write 0x%02x to 0x%02x\n", seq[i], seq[i+1]);
		eusb2_i2c_write_reg(er, seq[i+1], seq[i]);
	}
}
#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
static void eusb2_repeater_tune_buf_init(void)
{
	int i;

	for (i = 0; i < TUNE_BUF_COUNT; i++)
		ter->tune_buf[i][0] = ter->tune_buf[i][1] = 0;
}

static void eusb2_repeater_tune_set(void)
{
	int i;
	u8 reg_val;

	mutex_lock(&ter->er_tune_lock);
	for (i = 0; i < ter->tune_buf_cnt; i++) {
		if (!ter->ur.is_host && ter->chip->repeater_type == NXP_REPEATER &&
			ter->tune_buf[i][0] == 0x2 && ter->tune_buf[i][1] == 0x03)
			pr_info("%s(): skip host test mode setting in NXP USB client mode\n");
		else
			eusb2_i2c_write_reg(ter, ter->tune_buf[i][0], ter->tune_buf[i][1]);

		usleep_range(1, 10);
		eusb2_i2c_read_reg(ter, ter->tune_buf[i][0], &reg_val);
		pr_info("%s(): [%d] 0x%x 0x%x (%d/%d)\n", __func__, i, ter->tune_buf[i][0],
			reg_val, ter->tune_buf_cnt, TUNE_BUF_COUNT);
		usleep_range(1, 2);
	}
	mutex_unlock(&ter->er_tune_lock);
}

static ssize_t eusb2_repeater_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char str[(TUNE_BUF_SIZE * TUNE_BUF_COUNT) + 35] = {0, };
	char str2[(TUNE_BUF_SIZE * TUNE_BUF_COUNT) + 35] = {0, };
	int i, ret;
	u8 reg_val;

	if (!ter) {
		pr_err("eusb2 repeater is NULL\n");
		return -ENODEV;
	}
	mutex_lock(&ter->er_tune_lock);
	sprintf(str, "\n Address Value - %s\n", ter->chip->repeater_type ? "NXP":"TI");
	if (ter->chip->repeater_type == NXP_REPEATER) {
		for (i = 0; i < TUNE_MAX_NXP; i++) {
			strcpy(str2, str);
			ret = eusb2_i2c_read_reg(ter, tune_map_nxp[i], &reg_val);
			if (ret < 0) {
				dev_err(ter->dev, "Failed to read reg:0x%02x ret=%d\n", tune_map_nxp[i], ret);
				mutex_unlock(&ter->er_tune_lock);
				return sprintf(buf, "Failed to read reg\n");
			}
			sprintf(str, "%s  0x%2x   0x%2x\n", str2, tune_map_nxp[i], reg_val);
		}
	} else {
		for (i = 0; i < TUNE_MAX_TI; i++) {
			strcpy(str2, str);
			ret = eusb2_i2c_read_reg(ter, tune_map_ti[i], &reg_val);
			if (ret < 0) {
				dev_err(ter->dev, "Failed to read reg:0x%02x ret=%d\n", tune_map_ti[i], ret);
				mutex_unlock(&ter->er_tune_lock);
				return sprintf(buf, "Failed to read reg\n");
			}
			sprintf(str, "%s  0x%2x   0x%2x\n", str2, tune_map_ti[i], reg_val);
		}
	}
	mutex_unlock(&ter->er_tune_lock);

	return sprintf(buf, "%s\n", str);
}

static ssize_t eusb2_repeater_tune_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 reg, val, reg_val;
	int i, ret;

	pr_info("%s buf=%s\n", __func__, buf);
	if (!ter) {
		pr_err("eusb2 repeater is NULL\n");
		return -ENODEV;
	}
	sscanf(buf, "%hhx %hhx", &reg, &val);
	mutex_lock(&ter->er_tune_lock);

	for (i = 0; i < ter->tune_buf_cnt; i++) {
		if (ter->tune_buf[i][0] == reg) {
			ret = eusb2_i2c_write_reg(ter, reg, val);
			if (ret < 0) {
				dev_err(ter->dev, "failed to write 0x%02x to reg: 0x%02x ret=%d\n", val, reg, ret);
				mutex_unlock(&ter->er_tune_lock);
				return ret;
			}
			ter->tune_buf[i][1] = val;
			usleep_range(1, 2);
			ret = eusb2_i2c_read_reg(ter, reg, &reg_val);
			if (ret < 0) {
				dev_err(ter->dev, "Failed to read reg:0x%02x ret=%d\n", reg, ret);
				mutex_unlock(&ter->er_tune_lock);
				return ret;
			}
			pr_info("%s(): [%d] 0x%x 0x%x (%d/%d)\n", __func__, i, reg,
				reg_val, ter->tune_buf_cnt, TUNE_BUF_COUNT);
			mutex_unlock(&ter->er_tune_lock);
			return size;
		}
	}
	if (ter->tune_buf_cnt < TUNE_BUF_COUNT) {
		ret = eusb2_i2c_write_reg(ter, reg, val);
		if (ret < 0) {
			dev_err(ter->dev, "failed to write 0x%02x to reg: 0x%02x ret=%d\n", val, reg, ret);
			mutex_unlock(&ter->er_tune_lock);
			return ret;
		}
		ter->tune_buf[i][0] = reg;
		ter->tune_buf[i][1] = val;
		usleep_range(1, 2);
		ret = eusb2_i2c_read_reg(ter, reg, &reg_val);
		if (ret < 0) {
			dev_err(ter->dev, "Failed to read reg:0x%02x ret=%d\n", reg, ret);
			mutex_unlock(&ter->er_tune_lock);
			return ret;
		}
		pr_info("%s(): [%d] 0x%x 0x%x (%d/%d)\n", __func__, i, reg,
			reg_val, ter->tune_buf_cnt, TUNE_BUF_COUNT);
		ter->tune_buf_cnt++;
	} else
		pr_info("%s(): tuning count is full\n", __func__);

	mutex_unlock(&ter->er_tune_lock);

	return size;
}

static DEVICE_ATTR_RW(eusb2_repeater_tune);

static struct attribute *eusb2_repeater_attributes[] = {
	&dev_attr_eusb2_repeater_tune.attr,
	NULL
};

const struct attribute_group eusb2_repeater_sysfs_group = {
	.attrs = eusb2_repeater_attributes,
};
#endif

static int eusb2_repeater_power(struct eusb2_repeater *er, bool on)
{
	struct repeater_vreg *vdd18 = er->vdd18;
	struct repeater_vreg *vdd3 = er->vdd3;
	int ret = 0;

	dev_dbg(er->ur.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", er->power_enabled);

	if (er->power_enabled == on) {
		dev_dbg(er->ur.dev, "regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto disable_vdd3;

	ret = regulator_set_load(vdd18->reg, vdd18->max_uA);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd18:%d\n", ret);
		goto err_vdd18;
	}

	ret = regulator_set_voltage(vdd18->reg, vdd18->min_uV, vdd18->max_uV);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd18:%d\n", ret);
		goto put_vdd18_lpm;
	}

	ret = regulator_enable(vdd18->reg);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd18:%d\n", ret);
		goto unset_vdd18;
	}

	ret = regulator_set_load(vdd3->reg, vdd3->max_uA);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd3:%d\n", ret);
		goto disable_vdd18;
	}

	ret = regulator_set_voltage(vdd3->reg, vdd3->min_uV, vdd3->max_uV);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd3:%d\n", ret);
		goto put_vdd3_lpm;
	}

	ret = regulator_enable(vdd3->reg);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd3:%d\n", ret);
		goto unset_vdd3;
	}

	er->power_enabled = true;
	dev_dbg(er->ur.dev, "eUSB2 repeater regulators are turned ON.\n");
	return ret;

disable_vdd3:
	ret = regulator_disable(vdd3->reg);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd3:%d\n", ret);

unset_vdd3:
	ret = regulator_set_voltage(vdd3->reg, 0, vdd3->max_uV);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd3:%d\n", ret);

put_vdd3_lpm:
	ret = regulator_set_load(vdd3->reg, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set (0) HPM of vdd3\n");

disable_vdd18:
	ret = regulator_disable(vdd18->reg);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd18:%d\n", ret);

unset_vdd18:
	ret = regulator_set_voltage(vdd18->reg, 0, vdd18->max_uV);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd18:%d\n", ret);

put_vdd18_lpm:
	ret = regulator_set_load(vdd18->reg, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set LPM of vdd18\n");

	/* case handling when regulator turning on failed */
	if (!er->power_enabled)
		return -EINVAL;

err_vdd18:
	er->power_enabled = false;
	dev_dbg(er->ur.dev, "eUSB2 repeater's regulators are turned OFF.\n");
	return ret;
}

static int eusb2_repeater_init(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);
	const struct i2c_repeater_chip *chip = er->chip;
	u8 reg_val;

	switch (chip->repeater_type) {
	case TI_REPEATER:
		eusb2_i2c_read_reg(er, REV_ID, &reg_val);
		/* If the repeater revision is B1 disable auto-resume WA */
		if (reg_val == 0x03)
			ur->flags |= UR_AUTO_RESUME_SUPPORTED;
		break;
	case NXP_REPEATER:
		eusb2_i2c_read_reg(er, REVISION_ID, &reg_val);
		break;
	case DIODES_REPEATER_PI3EUSB1100:
		eusb2_i2c_read_reg(er, DIODES_PI3EUSB1100_REV_ID, &reg_val);
		break;
	default:
		dev_err(er->ur.dev, "Invalid repeater\n");
	}

	dev_info(er->ur.dev, "eUSB2 repeater version = 0x%x ur->flags:0x%x\n", reg_val, ur->flags);

	/* override init sequence using devicetree based values */
#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	dev_info(er->ur.dev, "%s %s mode\n",
		er->chip->repeater_type ? "NXP":"TI", er->ur.is_host ? "HOST":"CLIENT");
	if (er->param_host_override_seq_cnt && er->ur.is_host)
		eusb2_repeater_update_seq(er, er->param_host_override_seq,
					er->param_host_override_seq_cnt);
	else
#endif
	if (er->param_override_seq_cnt)
		eusb2_repeater_update_seq(er, er->param_override_seq,
					er->param_override_seq_cnt);
#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	if (er->tune_buf_cnt && er->er_tune_init_done)
		eusb2_repeater_tune_set();
#endif

	dev_info(er->ur.dev, "eUSB2 repeater init\n");

	return 0;
}

static int eusb2_repeater_reset(struct usb_repeater *ur, bool bring_out_of_reset)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	dev_dbg(ur->dev, "reset gpio:%s\n",
			bring_out_of_reset ? "assert" : "deassert");
	gpiod_set_value_cansleep(er->reset_gpiod, bring_out_of_reset);
	return 0;
}

static int eusb2_repeater_powerup(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, true);
}

static int eusb2_repeater_powerdown(struct usb_repeater *ur)
{
	struct eusb2_repeater *er =
			container_of(ur, struct eusb2_repeater, ur);

	return eusb2_repeater_power(er, false);
}

static struct i2c_repeater_chip repeater_chip[] = {
	[NXP_REPEATER] = {
		.repeater_type = NXP_REPEATER,
	},
	[TI_REPEATER] = {
		.repeater_type = TI_REPEATER,
	},
	[DIODES_REPEATER_PI3EUSB1100] = {
		.repeater_type = DIODES_REPEATER_PI3EUSB1100,
	}
};

static const struct of_device_id eusb2_repeater_id_table[] = {
	{
		.compatible = "nxp,eusb2-repeater",
		.data = &repeater_chip[NXP_REPEATER]
	},
	{
		.compatible = "ti,eusb2-repeater",
		.data = &repeater_chip[TI_REPEATER]
	},
	{
		.compatible = "diodes,eusb2-repeater-PI3EUSB1100",
		.data = &repeater_chip[DIODES_REPEATER_PI3EUSB1100]
	},
	{ },
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_id_table);

static int eusb2_repeater_i2c_probe(struct i2c_client *client)
{
	struct eusb2_repeater *er;
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	int ret = 0, num_elem;
#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	struct device *eusb2_repeater_device;
#endif

	pr_info("%s\n", __func__);
	er = devm_kzalloc(dev, sizeof(*er), GFP_KERNEL);
	if (!er) {
		ret = -ENOMEM;
		goto err_probe;
	}

	er->dev = dev;
	match = of_match_node(eusb2_repeater_id_table, dev->of_node);
	if (!match) {
		dev_err(dev, "eUSB2 repeater node not found.\n");
		return -EINVAL;
	}

	er->chip = match->data;

	er->regmap = devm_regmap_init_i2c(client, &eusb2_i2c_regmap);
	if (!er->regmap) {
		dev_err(dev, "failed to allocate register map\n");
		ret = -EINVAL;
		goto err_probe;
	}

	devm_regmap_qti_debugfs_register(er->dev, er->regmap);
	i2c_set_clientdata(client, er);

	ret = of_property_read_u16(dev->of_node, "reg", &er->reg_base);
	if (ret < 0) {
		dev_err(dev, "failed to get reg base address:%d\n", ret);
		goto err_probe;
	}

	ret = repeater_setup_vreg(er);
	if (ret)
		goto err_probe;

	er->reset_gpiod = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(er->reset_gpiod)) {
		ret = PTR_ERR(er->reset_gpiod);
		goto err_probe;
	}

	num_elem = of_property_count_elems_of_size(dev->of_node, "qcom,param-override-seq",
				sizeof(*er->param_override_seq));
	if (num_elem > 0) {
		if (num_elem % 2) {
			dev_err(dev, "invalid param_override_seq_len\n");
			ret = -EINVAL;
			goto err_probe;
		}

		er->param_override_seq_cnt = num_elem;
		er->param_override_seq = devm_kcalloc(dev,
				er->param_override_seq_cnt,
				sizeof(*er->param_override_seq), GFP_KERNEL);
		if (!er->param_override_seq) {
			ret = -ENOMEM;
			goto err_probe;
		}

		ret = of_property_read_u32_array(dev->of_node,
				"qcom,param-override-seq",
				er->param_override_seq,
				er->param_override_seq_cnt);
		if (ret) {
			dev_err(dev, "qcom,param-override-seq read failed %d\n",
									ret);
			goto err_probe;
		}
	}

#if IS_ENABLED(CONFIG_USB_NOTIFIER)
	num_elem = of_property_count_elems_of_size(dev->of_node, "qcom,param-host-override-seq",
				sizeof(*er->param_host_override_seq));
	if (num_elem > 0) {
		if (num_elem % 2) {
			dev_err(dev, "invalid param_host_override_seq_len\n");
			ret = -EINVAL;
			goto err_probe;
		}

		er->param_host_override_seq_cnt = num_elem;
		er->param_host_override_seq = devm_kcalloc(dev,
				er->param_host_override_seq_cnt,
				sizeof(*er->param_host_override_seq), GFP_KERNEL);
		if (!er->param_host_override_seq) {
			ret = -ENOMEM;
			goto err_probe;
		}

		ret = of_property_read_u32_array(dev->of_node,
				"qcom,param-host-override-seq",
				er->param_host_override_seq,
				er->param_host_override_seq_cnt);
		if (ret) {
			dev_err(dev, "qcom,param-host-override-seq read failed %d\n",
									ret);
			goto err_probe;
		}
	}
#endif

	er->ur.dev = dev;

	er->ur.init		= eusb2_repeater_init;
	er->ur.reset		= eusb2_repeater_reset;
	er->ur.powerup		= eusb2_repeater_powerup;
	er->ur.powerdown	= eusb2_repeater_powerdown;

	ret = usb_add_repeater_dev(&er->ur);
	if (ret)
		goto err_probe;

#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	ter = er;
	er->tune_buf_cnt = 0;
	er->er_tune_init_done = true;
	eusb2_repeater_tune_buf_init();
	mutex_init(&er->er_tune_lock);
	eusb2_repeater_device = sec_device_create(NULL, "usb_repeater");
	if (IS_ERR(eusb2_repeater_device))
		pr_err("%s Failed to create device(usb_repeater)!\n", __func__);


	ret = sysfs_create_group(&eusb2_repeater_device->kobj, &eusb2_repeater_sysfs_group);
	if (ret)
		pr_err("%s: usb_repeater sysfs_create_group fail, ret %d", __func__, ret);
#endif
	pr_info("%s %s done\n", __func__, er->chip->repeater_type ? "NXP":"TI");
	return 0;

err_probe:
	pr_info("%s failed. ret(%d)\n", __func__, ret);
	return ret;
}

static int eusb2_repeater_i2c_remove(struct i2c_client *client)
{
	struct eusb2_repeater *er = i2c_get_clientdata(client);

	if (!er)
		return 0;

#if IS_ENABLED(CONFIG_USB_PHY_TUNING_QCOM)
	mutex_destroy(&er->er_tune_lock);
#endif
	usb_remove_repeater_dev(&er->ur);
	eusb2_repeater_power(er, false);
	return 0;
}

static struct i2c_driver eusb2_i2c_repeater_driver = {
	.probe_new	= eusb2_repeater_i2c_probe,
	.remove		= eusb2_repeater_i2c_remove,
	.driver = {
		.name	= "eusb2-repeater",
		.of_match_table = of_match_ptr(eusb2_repeater_id_table),
	},
};

module_i2c_driver(eusb2_i2c_repeater_driver);

MODULE_DESCRIPTION("eUSB2 i2c repeater driver");
MODULE_LICENSE("GPL v2");
