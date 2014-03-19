/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

/* Status register bits */
#define STATUS_POR_BIT         (1 << 1)
#define STATUS_BST_BIT         (1 << 3)
#define STATUS_VMN_BIT         (1 << 8)
#define STATUS_TMN_BIT         (1 << 9)
#define STATUS_SMN_BIT         (1 << 10)
#define STATUS_BI_BIT          (1 << 11)
#define STATUS_VMX_BIT         (1 << 12)
#define STATUS_TMX_BIT         (1 << 13)
#define STATUS_SMX_BIT         (1 << 14)
#define STATUS_BR_BIT          (1 << 15)

/* Interrupt mask bits */
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define STATUS_INTR_VMIN_BIT	(1 << 8)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define MODEL_UNLOCK1	0X0059
#define MODEL_UNLOCK2	0X00C4
#define MODEL_LOCK1		0X0000
#define MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

#define MAX17042_IC_VERSION	0x0092
#define MAX17047_IC_VERSION	0x00AC	/* same for max17050 */

#define INIT_DATA_PROPERTY	"maxim,regs-init-data"

struct max17042_chip {
	struct i2c_client *client;
	struct regmap *regmap;
	struct power_supply battery;
	enum max170xx_chip_type chip_type;
	struct max17042_platform_data *pdata;
	struct work_struct work;
	int    init_complete;
	bool batt_undervoltage;
#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	struct dentry *debugfs_root;
	u8 debugfs_addr;
#endif
};

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
};

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	struct regmap *map = chip->regmap;
	int ret;
	u32 data;

	if (!chip->init_complete)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		ret = regmap_read(map, MAX17042_STATUS, &data);
		if (ret < 0)
			return ret;

		if (data & MAX17042_STATUS_BattAbsent)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = regmap_read(map, MAX17042_Cycles, &data);
		if (ret < 0)
			return ret;

		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = regmap_read(map, MAX17042_MinMaxVolt, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (chip->chip_type == MAX17042)
			ret = regmap_read(map, MAX17042_V_empty, &data);
		else
			ret = regmap_read(map, MAX17047_V_empty, &data);
		if (ret < 0)
			return ret;

		val->intval = data >> 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = regmap_read(map, MAX17042_VCELL, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = regmap_read(map, MAX17042_AvgVCELL, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = regmap_read(map, MAX17042_OCVInternal, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (chip->pdata->batt_undervoltage_zero_soc &&
		    chip->batt_undervoltage) {
			val->intval = 0;
			break;
		}

		ret = regmap_read(map, MAX17042_RepSOC, &data);
		if (ret < 0)
			return ret;

		data >>= 8;
		if (data == 0 &&
		    chip->pdata->batt_undervoltage_zero_soc &&
		    !chip->batt_undervoltage)
			val->intval = 1;
		else
			val->intval = data;

		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = regmap_read(map, MAX17042_FullCAP, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = regmap_read(map, MAX17042_QH, &data);
		if (ret < 0)
			return ret;

		val->intval = data * 1000 / 2;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = regmap_read(map, MAX17042_TEMP, &data);
		if (ret < 0)
			return ret;

		val->intval = data;
		/* The value is signed. */
		if (val->intval & 0x8000) {
			val->intval = (0x7fff & ~val->intval) + 1;
			val->intval *= -1;
		}
		/* The value is converted into deci-centigrade scale */
		/* Units of LSB = 1 / 256 degree Celsius */
		val->intval = val->intval * 10 / 256;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_Current, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->pdata->enable_current_sense) {
			ret = regmap_read(map, MAX17042_AvgCurrent, &data);
			if (ret < 0)
				return ret;

			val->intval = data;
			if (val->intval & 0x8000) {
				/* Negative */
				val->intval = ~val->intval & 0x7fff;
				val->intval++;
				val->intval *= -1;
			}
			val->intval *= 1562500 / chip->pdata->r_sns;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17042_write_verify_reg(struct regmap *map, u8 reg, u32 value)
{
	int retries = 8;
	int ret;
	u32 read_value;

	do {
		ret = regmap_write(map, reg, value);
		regmap_read(map, reg, &read_value);
		if (read_value != value) {
			ret = -EIO;
			retries--;
		}
	} while (retries && read_value != value);

	if (ret < 0)
		pr_err("%s: err %d\n", __func__, ret);

	return ret;
}

static inline void max17042_override_por(struct regmap *map,
					 u8 reg, u16 value)
{
	if (value)
		regmap_write(map, reg, value);
}

static inline void max10742_unlock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	regmap_write(map, MAX17042_MLOCKReg1, MODEL_UNLOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_UNLOCK2);
}

static inline void max10742_lock_model(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_MLOCKReg1, MODEL_LOCK1);
	regmap_write(map, MAX17042_MLOCKReg2, MODEL_LOCK2);
}

static inline void max17042_write_model_data(struct max17042_chip *chip,
					u8 addr, int size)
{
	struct regmap *map = chip->regmap;
	int i;
	for (i = 0; i < size; i++)
		regmap_write(map, addr + i,
			chip->pdata->config_data->cell_char_tbl[i]);
}

static inline void max17042_read_model_data(struct max17042_chip *chip,
					u8 addr, u32 *data, int size)
{
	struct regmap *map = chip->regmap;
	int i;

	for (i = 0; i < size; i++)
		regmap_read(map, addr + i, &data[i]);
}

static inline int max17042_model_data_compare(struct max17042_chip *chip,
					u16 *data1, u16 *data2, int size)
{
	int i;

	if (memcmp(data1, data2, size)) {
		dev_err(&chip->client->dev, "%s compare failed\n", __func__);
		for (i = 0; i < size; i++)
			dev_info(&chip->client->dev, "0x%x, 0x%x",
				data1[i], data2[i]);
		dev_info(&chip->client->dev, "\n");
		return -EINVAL;
	}
	return 0;
}

static int max17042_init_model(struct max17042_chip *chip)
{
	int ret;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u32 *temp_data;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max10742_unlock_model(chip);
	max17042_write_model_data(chip, MAX17042_MODELChrTbl,
				table_size);
	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);

	ret = max17042_model_data_compare(
		chip,
		chip->pdata->config_data->cell_char_tbl,
		(u16 *)temp_data,
		table_size);

	max10742_lock_model(chip);
	kfree(temp_data);

	return ret;
}

static int max17042_verify_model_lock(struct max17042_chip *chip)
{
	int i;
	int table_size = ARRAY_SIZE(chip->pdata->config_data->cell_char_tbl);
	u32 *temp_data;
	int ret = 0;

	temp_data = kcalloc(table_size, sizeof(*temp_data), GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	max17042_read_model_data(chip, MAX17042_MODELChrTbl, temp_data,
				table_size);
	for (i = 0; i < table_size; i++)
		if (temp_data[i])
			ret = -EINVAL;

	kfree(temp_data);
	return ret;
}

static void max17042_write_config_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	regmap_write(map, MAX17042_CONFIG, config->config);
	regmap_write(map, MAX17042_LearnCFG, config->learn_cfg);
	regmap_write(map, MAX17042_FilterCFG,
			config->filter_cfg);
	regmap_write(map, MAX17042_RelaxCFG, config->relax_cfg);
	if (chip->chip_type == MAX17047)
		regmap_write(map, MAX17047_FullSOCThr,
						config->full_soc_thresh);
}

static void  max17042_write_custom_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_write_verify_reg(map, MAX17042_TempCo,	config->tcompc0);
	max17042_write_verify_reg(map, MAX17042_ICHGTerm, config->ichgt_term);
	if (chip->chip_type == MAX17042) {
		regmap_write(map, MAX17042_EmptyTempCo,	config->empty_tempco);
		max17042_write_verify_reg(map, MAX17042_K_empty0,
					config->kempty0);
	} else {
		max17042_write_verify_reg(map, MAX17047_QRTbl00,
						config->qrtbl00);
		max17042_write_verify_reg(map, MAX17047_QRTbl10,
						config->qrtbl10);
		max17042_write_verify_reg(map, MAX17047_QRTbl20,
						config->qrtbl20);
		max17042_write_verify_reg(map, MAX17047_QRTbl30,
						config->qrtbl30);
	}
}

static void max17042_update_capacity_regs(struct max17042_chip *chip)
{
	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;

	max17042_write_verify_reg(map, MAX17042_FullCAP,
				config->fullcap);
	regmap_write(map, MAX17042_DesignCap, config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
				config->fullcapnom);
}

static void max17042_reset_vfsoc0_reg(struct max17042_chip *chip)
{
	unsigned int vfSoc;
	struct regmap *map = chip->regmap;

	regmap_read(map, MAX17042_VFSOC, &vfSoc);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(map, MAX17042_VFSOC0, vfSoc);
	regmap_write(map, MAX17042_VFSOC0Enable, VFSOC0_LOCK);
}

static void max17042_load_new_capacity_params(struct max17042_chip *chip)
{
	u16 rep_cap, dq_acc, vfSoc;
	u32 rem_cap;

	struct max17042_config_data *config = chip->pdata->config_data;
	struct regmap *map = chip->regmap;


	regmap_read(map, MAX17042_VFSOC, &vfSoc);

	/* vfSoc needs to shifted by 8 bits to get the
	 * perc in 1% accuracy, to get the right rem_cap multiply
	 * fullcapnom by vfSoc and devide by 100
	 */

	rem_cap = ((vfSoc >> 8) * config->fullcapnom) / 100;
	max17042_write_verify_reg(chip->client, MAX17042_RemCap, (u16)rem_cap);

	rep_cap = rem_cap;
	max17042_write_verify_reg(map, MAX17042_RepCap, rep_cap);

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = config->fullcap / dQ_ACC_DIV;
	max17042_write_verify_reg(map, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(map, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(map, MAX17042_FullCAP,
			config->fullcap);
	regmap_write(map, MAX17042_DesignCap,
			config->design_cap);
	max17042_write_verify_reg(map, MAX17042_FullCAPNom,
			config->fullcapnom);
	/* Update SOC register with new SOC */
	regmap_write(map, MAX17042_RepSOC, vfSoc);
}

/*
 * Block write all the override values coming from platform data.
 * This function MUST be called before the POR initialization proceedure
 * specified by maxim.
 */
static inline void max17042_override_por_values(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	struct max17042_config_data *config = chip->pdata->config_data;

	max17042_override_por(map, MAX17042_TGAIN, config->tgain);
	max17042_override_por(map, MAx17042_TOFF, config->toff);
	max17042_override_por(map, MAX17042_CGAIN, config->cgain);
	max17042_override_por(map, MAX17042_COFF, config->coff);

	max17042_override_por(map, MAX17042_VALRT_Th, config->valrt_thresh);
	max17042_override_por(map, MAX17042_TALRT_Th, config->talrt_thresh);
	max17042_override_por(map, MAX17042_SALRT_Th,
						config->soc_alrt_thresh);
	max17042_override_por(map, MAX17042_CONFIG, config->config);
	max17042_override_por(map, MAX17042_SHDNTIMER, config->shdntimer);

	max17042_override_por(map, MAX17042_DesignCap, config->design_cap);
	max17042_override_por(map, MAX17042_ICHGTerm, config->ichgt_term);

	max17042_override_por(map, MAX17042_AtRate, config->at_rate);
	max17042_override_por(map, MAX17042_LearnCFG, config->learn_cfg);
	max17042_override_por(map, MAX17042_FilterCFG, config->filter_cfg);
	max17042_override_por(map, MAX17042_RelaxCFG, config->relax_cfg);
	max17042_override_por(map, MAX17042_MiscCFG, config->misc_cfg);
	max17042_override_por(map, MAX17042_MaskSOC, config->masksoc);

	max17042_override_por(map, MAX17042_FullCAP, config->fullcap);
	max17042_override_por(map, MAX17042_FullCAPNom, config->fullcapnom);
	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_SOC_empty,
						config->socempty);
	max17042_override_por(map, MAX17042_LAvg_empty, config->lavg_empty);
	max17042_override_por(map, MAX17042_dQacc, config->dqacc);
	max17042_override_por(map, MAX17042_dPacc, config->dpacc);

	if (chip->chip_type == MAX17042)
		max17042_override_por(map, MAX17042_V_empty, config->vempty);
	else
		max17042_override_por(map, MAX17047_V_empty, config->vempty);
	max17042_override_por(map, MAX17042_TempNom, config->temp_nom);
	max17042_override_por(map, MAX17042_TempLim, config->temp_lim);
	max17042_override_por(map, MAX17042_FCTC, config->fctc);
	max17042_override_por(map, MAX17042_RCOMP0, config->rcomp0);
	max17042_override_por(map, MAX17042_TempCo, config->tcompc0);
	if (chip->chip_type == MAX17042) {
		max17042_override_por(map, MAX17042_EmptyTempCo,
						config->empty_tempco);
		max17042_override_por(map, MAX17042_K_empty0,
						config->kempty0);
	}
}

static int max17042_init_chip(struct max17042_chip *chip)
{
	struct regmap *map = chip->regmap;
	int ret;
	int val;

	max17042_override_por_values(chip);
	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	/* Initialize configaration */
	max17042_write_config_regs(chip);

	/* write cell characterization data */
	ret = max17042_init_model(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s init failed\n",
			__func__);
		return -EIO;
	}

	ret = max17042_verify_model_lock(chip);
	if (ret) {
		dev_err(&chip->client->dev, "%s lock verify failed\n",
			__func__);
		return -EIO;
	}
	/* write custom parameters */
	max17042_write_custom_regs(chip);

	/* update capacity params */
	max17042_update_capacity_regs(chip);

	/* delay must be atleast 350mS to allow VFSOC
	 * to be calculated from the new configuration
	 */
	msleep(350);

	/* reset vfsoc0 reg */
	max17042_reset_vfsoc0_reg(chip);

	/* load new capacity params */
	max17042_load_new_capacity_params(chip);

	/* Init complete, Clear the POR bit */
	regmap_read(map, MAX17042_STATUS, &val);
	regmap_write(map, MAX17042_STATUS, val & (~STATUS_POR_BIT));
	return 0;
}

static void max17042_set_soc_threshold(struct max17042_chip *chip, u16 off)
{
	struct regmap *map = chip->regmap;
	u32 soc, soc_tr;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'off' perc change in the soc
	 */
	regmap_read(map, MAX17042_RepSOC, &soc);
	soc >>= 8;
	soc_tr = (soc + off) << 8;
	soc_tr |= (soc - off);
	regmap_write(map, MAX17042_SALRT_Th, soc_tr);
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;
	u32 val;

	regmap_read(chip->regmap, MAX17042_STATUS, &val);
	if ((val & STATUS_INTR_SOCMIN_BIT) ||
		(val & STATUS_INTR_SOCMAX_BIT)) {
		dev_info(&chip->client->dev, "SOC threshold INTR\n");
		max17042_set_soc_threshold(chip, 1);
	}

	if (val & STATUS_INTR_VMIN_BIT) {
		dev_info(&chip->client->dev, "Battery undervoltage INTR\n");
		chip->batt_undervoltage = true;
	}

	power_supply_changed(&chip->battery);
	return IRQ_HANDLED;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, work);
	int ret;

	/* Initialize registers according to values from the platform data */
	if (chip->pdata->enable_por_init && chip->pdata->config_data) {
		ret = max17042_init_chip(chip);
		if (ret)
			return;
	}

	chip->init_complete = 1;
}

struct max17042_config_data eg30_lg_config = {
	/* External current sense resistor value in milli-ohms */
	.cur_sense_val = 10,

	/* A/D measurement */
	.tgain = 0xE71C,	/* 0x2C */
	.toff = 0x251A,		/* 0x2D */
	.cgain = 0x4000,	/* 0x2E */
	.coff = 0x0000,		/* 0x2F */

	/* Alert / Status */
	.valrt_thresh = 0xFF00,	/* 0x01 */
	.talrt_thresh = 0x7F80,	/* 0x02 */
	.soc_alrt_thresh = 0xFF00,	/* 0x03 */
	.config = 0x0210,		/* 0x01D */
	.shdntimer = 0xE000,	/* 0x03F */

	/* App data */
	.full_soc_thresh = 0x6200,	/* 0x13 */
	.design_cap = 4172,	/* 0x18 0.5 mAh per bit */
	.ichgt_term = 0x01C0,	/* 0x1E */

	/* MG3 config */
	.filter_cfg = 0x87A4,	/* 0x29 */

	/* MG3 save and restore */
	.fullcap = 4172,	/* 0x10 0.5 mAh per bit */
	.fullcapnom = 4172,	/* 0x23 0.5 mAh per bit */
	.qrtbl00 = 0x1E01,	/* 0x12 */
	.qrtbl10 = 0x1281,	/* 0x22 */
	.qrtbl20 = 0x0781,	/* 0x32 */
	.qrtbl30 = 0x0681,	/* 0x42 */

	/* Cell technology from power_supply.h */
	.cell_technology = POWER_SUPPLY_TECHNOLOGY_LION,

	/* Cell Data */
	.vempty = 0x7D5A,		/* 0x12 */
	.temp_nom = 0x1400,	/* 0x24 */
	.rcomp0 = 0x004A,		/* 0x38 */
	.tcompc0 = 0x243A,	/* 0x39 */
	.cell_char_tbl = {
		0x9B00,
		0xA470,
		0xB2E0,
		0xB7B0,
		0xB9B0,
		0xBB70,
		0xBC20,
		0xBD00,
		0xBE10,
		0xC060,
		0xC220,
		0xC750,
		0xCB80,
		0xCF10,
		0xD2A0,
		0xD840,
		0x0070,
		0x0020,
		0x0900,
		0x0D00,
		0x0B00,
		0x1B40,
		0x1B00,
		0x2030,
		0x0FB0,
		0x0BD0,
		0x08F0,
		0x08D0,
		0x06D0,
		0x06D0,
		0x07D0,
		0x07D0,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
		0x0180,
	},
};

#ifdef CONFIG_OF
static  struct gpio *
max17042_get_gpio_list(struct device *dev, int *num_gpio_list)
{
	struct device_node *np = dev->of_node;
	struct gpio *gpio_list;
	int i, num_gpios, gpio_list_size;
	enum of_gpio_flags flags;

	if (!np)
		return NULL;

	num_gpios = of_gpio_count(np);
	if (num_gpios <= 0)
		return NULL;

	gpio_list_size = sizeof(struct gpio) * num_gpios;
	gpio_list = devm_kzalloc(dev, gpio_list_size, GFP_KERNEL);

	if (!gpio_list)
		return NULL;

	*num_gpio_list = num_gpios;
	for (i = 0; i < num_gpios; i++) {
		gpio_list[i].gpio = of_get_gpio_flags(np, i, &flags);
		gpio_list[i].flags = flags;
		of_property_read_string_index(np, "gpio-names", i,
					      &gpio_list[i].label);
	}

	return gpio_list;
}

static struct max17042_reg_data *
max17042_get_init_data(struct device *dev, int *num_init_data)
{
	struct device_node *np = dev->of_node;
	const __be32 *property;
	static struct max17042_reg_data *init_data;
	int i, lenp, num_cells, init_data_size;

	if (!np)
		return NULL;

	property = of_get_property(np, INIT_DATA_PROPERTY, &lenp);

	if (!property || lenp <= 0)
		return NULL;

	/*
	 * Check data validity and whether number of cells is even
	 */
	if (lenp % sizeof(*property)) {
		dev_err(dev, "%s has invalid data\n", INIT_DATA_PROPERTY);
		return NULL;
	}

	num_cells = lenp / sizeof(*property);
	if (num_cells % 2) {
		dev_err(dev, "%s must have even number of cells\n",
			INIT_DATA_PROPERTY);
		return NULL;
	}

	*num_init_data = num_cells / 2;
	init_data_size = sizeof(struct max17042_reg_data) * (num_cells / 2);
	init_data = (struct max17042_reg_data *)
		    devm_kzalloc(dev, init_data_size, GFP_KERNEL);

	if (init_data) {
		for (i = 0; i < num_cells / 2; i++) {
			init_data[i].addr = be32_to_cpu(property[2 * i]);
			init_data[i].data = be32_to_cpu(property[2 * i + 1]);
		}
	}

	return init_data;
}

static int max17042_get_cell_char_tbl(struct device *dev,
				      struct device_node *np,
				      struct max17042_config_data *config_data)
{
	const __be16 *property;
	int i, lenp;

	property = of_get_property(np, CELL_CHAR_TBL_PROPERTY, &lenp);
	if (!property)
		return -ENODEV ;

	if (lenp != sizeof(*property) * MAX17042_CHARACTERIZATION_DATA_SIZE) {
		dev_err(dev, "%s must have %d cells\n", CELL_CHAR_TBL_PROPERTY,
			MAX17042_CHARACTERIZATION_DATA_SIZE);
		return -EINVAL;
	}

	for (i = 0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
		config_data->cell_char_tbl[i] = be16_to_cpu(property[i]);

	return 0;
}

static int max17042_cfg_rqrd_prop(struct device *dev,
				  struct device_node *np,
				  struct max17042_config_data *config_data)
{
	if (of_property_read_u16(np, CONFIG_PROPERTY,
				 &config_data->config))
		return -EINVAL;
	if (of_property_read_u16(np, FILTER_CFG_PROPERTY,
				 &config_data->filter_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, RELAX_CFG_PROPERTY,
				 &config_data->relax_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, LEARN_CFG_PROPERTY,
				 &config_data->learn_cfg))
		return -EINVAL;
	if (of_property_read_u16(np, FULL_SOC_THRESH_PROPERTY,
				 &config_data->full_soc_thresh))
		return -EINVAL;
	if (of_property_read_u16(np, RCOMP0_PROPERTY,
				 &config_data->rcomp0))
		return -EINVAL;
	if (of_property_read_u16(np, TCOMPC0_PROPERTY,
				 &config_data->tcompc0))
		return -EINVAL;
	if (of_property_read_u16(np, ICHGT_TERM_PROPERTY,
				 &config_data->ichgt_term))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL00_PROPERTY,
				 &config_data->qrtbl00))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL10_PROPERTY,
				 &config_data->qrtbl10))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL20_PROPERTY,
				 &config_data->qrtbl20))
		return -EINVAL;
	if (of_property_read_u16(np, QRTBL30_PROPERTY,
				 &config_data->qrtbl30))
		return -EINVAL;
	if (of_property_read_u16(np, FULLCAP_PROPERTY,
				 &config_data->fullcap))
		return -EINVAL;
	if (of_property_read_u16(np, DESIGN_CAP_PROPERTY,
				 &config_data->design_cap))
		return -EINVAL;
	if (of_property_read_u16(np, FULLCAPNOM_PROPERTY,
				 &config_data->fullcapnom))
		return -EINVAL;

	return max17042_get_cell_char_tbl(dev, np, config_data);
}

static struct max17042_config_data *
max17042_get_config_data(struct device *dev)
{
	struct max17042_config_data *config_data;
	struct device_node *np = dev->of_node;

	if (!np)
		return NULL;

	np = of_get_child_by_name(np, CONFIG_NODE);
	if (!np)
		return NULL;

	config_data = devm_kzalloc(dev, sizeof(*config_data), GFP_KERNEL);
	if (!config_data)
		return NULL;

	if (max17042_cfg_rqrd_prop(dev, np, config_data)) {
		devm_kfree(dev, config_data);
		return NULL;
	}

	config_data->cur_sense_val = 10;
	config_data->tgain = 0xE71C;
	config_data->toff = 0x251A;
	config_data->cgain = 0x4000;
	config_data->coff = 0x0000;
	config_data->valrt_thresh = 0xFF97;
	config_data->talrt_thresh = 0x7F80;
	config_data->soc_alrt_thresh = 0xFF00;
	config_data->config = 0x0214;
	config_data->shdntimer = 0xE000;
	config_data->cell_technology = POWER_SUPPLY_TECHNOLOGY_LION;
	config_data->vempty = 0x7D5A;
	config_data->temp_nom = 0x1400;

	return config_data;
}

static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop;
	struct max17042_platform_data *pdata;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->init_data = max17042_get_init_data(dev, &pdata->num_init_data);
	pdata->gpio_list = max17042_get_gpio_list(dev, &pdata->num_gpio_list);

	pdata->init_data = max17042_get_init_data(dev, &pdata->num_init_data);

	/*
	 * Require current sense resistor value to be specified for
	 * current-sense functionality to be enabled at all.
	 */
	if (of_property_read_u32(np, "maxim,rsns-microohm", &prop) == 0) {
		pdata->r_sns = prop;
		pdata->enable_current_sense = true;
	}

	pdata->batt_undervoltage_zero_soc =
		of_property_read_bool(np, "maxim,batt_undervoltage_zero_soc");

	pdata->config_data = max17042_get_config_data(dev);
	if (!pdata->config_data) {
		dev_warn(dev, "config data is missing\n");

		pdata->config_data = &eg30_lg_config;
		pdata->enable_por_init = true;
	}

	return pdata;
}
#else
static struct max17042_platform_data *
max17042_get_pdata(struct device *dev)
{
	return dev->platform_data;
}
#endif

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
static int max17042_debugfs_read_addr(void *data, u64 *val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	*val = chip->debugfs_addr;
	return 0;
}

static int max17042_debugfs_write_addr(void *data, u64 val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	chip->debugfs_addr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(addr_fops, max17042_debugfs_read_addr,
			max17042_debugfs_write_addr, "0x%02llx\n");

static int max17042_debugfs_read_data(void *data, u64 *val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	int ret = max17042_read_reg(chip->client, chip->debugfs_addr);

	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static int max17042_debugfs_write_data(void *data, u64 val)
{
	struct max17042_chip *chip = (struct max17042_chip *)data;
	return max17042_write_reg(chip->client, chip->debugfs_addr, val);
}
DEFINE_SIMPLE_ATTRIBUTE(data_fops, max17042_debugfs_read_data,
			max17042_debugfs_write_data, "0x%02llx\n");

static int max17042_debugfs_create(struct max17042_chip *chip)
{
	chip->debugfs_root = debugfs_create_dir(dev_name(&chip->client->dev),
						NULL);
	if (!chip->debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file("addr", S_IRUGO | S_IWUSR, chip->debugfs_root,
				 chip, &addr_fops))
		goto err_debugfs;

	if (!debugfs_create_file("data", S_IRUGO | S_IWUSR, chip->debugfs_root,
				 chip, &data_fops))
		goto err_debugfs;

	return 0;

err_debugfs:
	debugfs_remove_recursive(chip->debugfs_root);
	chip->debugfs_root = NULL;
	return -ENOMEM;
}
#endif

static struct regmap_config max17042_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int ret;
	int i;
	u32 val;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->regmap = devm_regmap_init_i2c(client, &max17042_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(&client->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	chip->pdata = max17042_get_pdata(&client->dev);
	if (!chip->pdata) {
		dev_err(&client->dev, "no platform data provided\n");
		return -EINVAL;
	}

	i2c_set_clientdata(client, chip);

	regmap_read(chip->regmap, MAX17042_DevName, &val);
	if (val == MAX17042_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17042 detected\n");
		chip->chip_type = MAX17042;
	} else if (val == MAX17047_IC_VERSION) {
		dev_dbg(&client->dev, "chip type max17047/50 detected\n");
		chip->chip_type = MAX17047;
	} else {
		dev_err(&client->dev, "device version mismatch: %x\n", val);
		return -EIO;
	}

	chip->battery.name		= "max170xx_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	/* When current is not measured,
	 * CURRENT_NOW and CURRENT_AVG properties should be invisible. */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 2;

	if (chip->pdata->r_sns == 0)
		chip->pdata->r_sns = MAX17042_DEFAULT_SNS_RESISTOR;

	if (chip->pdata->init_data)
		for (i = 0; i < chip->pdata->num_init_data; i++)
			regmap_write(chip->regmap,
					chip->pdata->init_data[i].addr,
					chip->pdata->init_data[i].data);

	if (!chip->pdata->enable_current_sense) {
		regmap_write(chip->regmap, MAX17042_CGAIN, 0x0000);
		regmap_write(chip->regmap, MAX17042_MiscCFG, 0x0003);
		regmap_write(chip->regmap, MAX17042_LearnCFG, 0x0007);
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	ret = gpio_request_array(chip->pdata->gpio_list,
				 chip->pdata->num_gpio_list);
	if (ret) {
		dev_err(&client->dev, "cannot request GPIOs\n");
		return ret;
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					max17042_thread_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (!ret) {
			regmap_read(chip->regmap, MAX17042_CONFIG, &val);
			val |= CONFIG_ALRT_BIT_ENBL;
			regmap_write(chip->regmap, MAX17042_CONFIG, val);
			max17042_set_soc_threshold(chip, 1);
		} else {
			client->irq = 0;
			dev_err(&client->dev, "%s(): cannot get IRQ\n",
				__func__);
		}
	}

	regmap_read(chip->regmap, MAX17042_STATUS, &val);
	if (val & STATUS_POR_BIT) {
		INIT_WORK(&chip->work, max17042_init_worker);
		schedule_work(&chip->work);
	} else {
		chip->init_complete = 1;
	}

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	ret = max17042_debugfs_create(chip);
	if (ret) {
		dev_err(&client->dev, "cannot create debugfs\n");
		return ret;
	}
#endif

	return 0;
}

static int max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

#ifdef CONFIG_BATTERY_MAX17042_DEBUGFS
	debugfs_remove_recursive(chip->debugfs_root);
#endif

	if (client->irq)
		free_irq(client->irq, chip);
	gpio_free_array(chip->pdata->gpio_list, chip->pdata->num_gpio_list);
	power_supply_unregister(&chip->battery);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17042_suspend(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	/*
	 * disable the irq and enable irq_wake
	 * capability to the interrupt line.
	 */
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}

	return 0;
}

static int max17042_resume(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
		/* re-program the SOC thresholds to 1% change */
		max17042_set_soc_threshold(chip, 1);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(max17042_pm_ops, max17042_suspend,
			max17042_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17042_dt_match[] = {
	{ .compatible = "maxim,max17042" },
	{ .compatible = "maxim,max17047" },
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17042_dt_match);
#endif

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ "max17047", 1 },
	{ "max17050", 2 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
		.of_match_table = of_match_ptr(max17042_dt_match),
		.pm	= &max17042_pm_ops,
	},
	.probe		= max17042_probe,
	.remove		= max17042_remove,
	.id_table	= max17042_id,
};
module_i2c_driver(max17042_i2c_driver);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
