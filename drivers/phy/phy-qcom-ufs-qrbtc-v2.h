/*
 * Copyright (c) 2013-2015, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef UFS_QCOM_PHY_QRBTC_V2_H_
#define UFS_QCOM_PHY_QRBTC_V2_H_

#include "phy-qcom-ufs-i.h"

/* QCOM UFS PHY control registers */
#define COM_OFF(x)	(0x000 + x)
#define PHY_OFF(x)	(0x700 + x)
#define PHY_USR(x)	(0x11000 + x)

#define UFS_PHY_PHY_START_OFFSET		PHY_OFF(0x00)
#define UFS_PHY_POWER_DOWN_CONTROL_OFFSET	PHY_OFF(0x04)

#define	QSERDES_COM_BIAS_EN_CLKBUFLR_EN_OFFSET	COM_OFF(0x20)
#define QSERDES_COM_SYSCLK_EN_SEL		COM_OFF(0x38)
#define QSERDES_COM_SYS_CLK_CTRL		COM_OFF(0x00)
#define QSERDES_COM_RES_CODE_TXBAND		COM_OFF(0x3C)
#define QSERDES_COM_PLL_VCOTAIL_EN		COM_OFF(0x04)
#define QSERDES_COM_PLL_CNTRL			COM_OFF(0x14)
#define QSERDES_COM_PLL_CLKEPDIV		COM_OFF(0xB0)
#define QSERDES_COM_RESETSM_CNTRL		COM_OFF(0x40)
#define QSERDES_COM_PLL_RXTXEPCLK_EN		COM_OFF(0xA8)
#define QSERDES_COM_PLL_CRCTRL			COM_OFF(0xAC)
#define QSERDES_COM_DEC_START1			COM_OFF(0x64)
#define QSERDES_COM_DEC_START2			COM_OFF(0xA4)
#define QSERDES_COM_DIV_FRAC_START1		COM_OFF(0x98)
#define QSERDES_COM_DIV_FRAC_START2		COM_OFF(0x9C)
#define QSERDES_COM_DIV_FRAC_START3		COM_OFF(0xA0)
#define QSERDES_COM_PLLLOCK_CMP1		COM_OFF(0x44)
#define QSERDES_COM_PLLLOCK_CMP2		COM_OFF(0x48)
#define QSERDES_COM_PLLLOCK_CMP3		COM_OFF(0x4C)
#define QSERDES_COM_PLLLOCK_CMP_EN		COM_OFF(0x50)
#define QSERDES_COM_PLL_IP_SETI			COM_OFF(0x18)
#define QSERDES_COM_PLL_CP_SETI			COM_OFF(0x24)
#define QSERDES_COM_PLL_IP_SETP			COM_OFF(0x28)
#define QSERDES_COM_PLL_CP_SETP			COM_OFF(0x2C)
#define QSERDES_COM_RESET_SM			COM_OFF(0xBC)
#define QSERDES_COM_PWM_CNTRL1			COM_OFF(0x280)
#define QSERDES_COM_PWM_CNTRL2			COM_OFF(0x284)
#define QSERDES_COM_PWM_NDIV			COM_OFF(0x288)
#define QSERDES_COM_CDR_CONTROL			COM_OFF(0x200)
#define QSERDES_COM_CDR_CONTROL_HALF		COM_OFF(0x298)
#define QSERDES_COM_CDR_CONTROL_QUARTER		COM_OFF(0x29C)
#define QSERDES_COM_SIGDET_CNTRL		COM_OFF(0x234)
#define QSERDES_COM_SIGDET_CNTRL2		COM_OFF(0x28C)
#define QSERDES_COM_UFS_CNTRL			COM_OFF(0x290)

/* QRBTC V2 USER REGISTERS */
#define U11_UFS_RESET_REG_OFFSET		PHY_USR(0x4)
#define U11_QRBTC_CONTROL_OFFSET		PHY_USR(0x18)

static struct ufs_qcom_phy_calibration phy_cal_table_rate_A[] = {
	UFS_QCOM_PHY_CAL_ENTRY(UFS_PHY_PHY_START_OFFSET, 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_BIAS_EN_CLKBUFLR_EN_OFFSET, 0x3F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SYSCLK_EN_SEL, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SYS_CLK_CTRL, 0x16),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RES_CODE_TXBAND, 0xC0),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_VCOTAIL_EN, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CNTRL, 0x88),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CLKEPDIV, 0x03),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_RESETSM_CNTRL, 0x30),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_RXTXEPCLK_EN, 0x10),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CRCTRL, 0x94),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START1, 0x98),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DEC_START2, 0x02),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START1, 0x8C),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START2, 0xAE),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_DIV_FRAC_START3, 0x1F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP1, 0xF7),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP2, 0x13),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP3, 0x00),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLLLOCK_CMP_EN, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETI, 0x01),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETI, 0x3B),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_IP_SETP, 0x0A),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PLL_CP_SETP, 0x04),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PWM_CNTRL1, 0x8F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PWM_CNTRL2, 0x61),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_PWM_NDIV, 0x4F),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_CDR_CONTROL, 0xF2),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_CDR_CONTROL_HALF, 0x2A),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_CDR_CONTROL_QUARTER, 0x2A),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SIGDET_CNTRL, 0xC0),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_SIGDET_CNTRL2, 0x07),
	UFS_QCOM_PHY_CAL_ENTRY(QSERDES_COM_UFS_CNTRL, 0x18),
};

/*
 * This structure represents the qrbtc-v2 specific phy.
 * common_cfg MUST remain the first field in this structure
 * in case extra fields are added. This way, when calling
 * get_ufs_qcom_phy() of generic phy, we can extract the
 * common phy structure (struct ufs_qcom_phy) out of it
 * regardless of the relevant specific phy.
 */
struct ufs_qcom_phy_qrbtc_v2 {
	struct ufs_qcom_phy common_cfg;
};

#endif
