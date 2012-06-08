/*
 * This file is part of wl18xx
 *
 * Copyright (C) 2011 Texas Instruments Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __WL18XX_CONF_H__
#define __WL18XX_CONF_H__

#define WL18XX_CONF_MAGIC	0x10e100ca
#define WL18XX_CONF_VERSION	(WLCORE_CONF_VERSION | 0x0002)
#define WL18XX_CONF_MASK	0x0000ffff
#define WL18XX_CONF_SIZE	(WLCORE_CONF_SIZE + \
				 sizeof(struct wl18xx_priv_conf))

#define NUM_OF_CHANNELS_11_ABG 150
#define NUM_OF_CHANNELS_11_P 7
#define WL18XX_NUM_OF_SUB_BANDS 9
#define SRF_TABLE_LEN 16
#define PIN_MUXING_SIZE 2

struct wl18xx_mac_and_phy_params {
	u8 phy_standalone;
	u8 rdl;
	u8 enable_clpc;
	u8 enable_tx_low_pwr_on_siso_rdl;
	u8 auto_detect;
	u8 dedicated_fem;

	u8 low_band_component;

	/* Bit 0: One Hot, Bit 1: Control Enable, Bit 2: 1.8V, Bit 3: 3V */
	u8 low_band_component_type;

	u8 high_band_component;

	/* Bit 0: One Hot, Bit 1: Control Enable, Bit 2: 1.8V, Bit 3: 3V */
	u8 high_band_component_type;
	u8 number_of_assembled_ant2_4;
	u8 number_of_assembled_ant5;
	u8 pin_muxing_platform_options[PIN_MUXING_SIZE];
	u8 external_pa_dc2dc;
	u8 tcxo_ldo_voltage;
	u8 xtal_itrim_val;
	u8 srf_state;
	u8 srf1[SRF_TABLE_LEN];
	u8 srf2[SRF_TABLE_LEN];
	u8 srf3[SRF_TABLE_LEN];
	u8 io_configuration;
	u8 sdio_configuration;
	u8 settings;
	u8 rx_profile;
	u8 per_chan_pwr_limit_arr_11abg[NUM_OF_CHANNELS_11_ABG];
	u8 pwr_limit_reference_11_abg;
	u8 per_chan_pwr_limit_arr_11p[NUM_OF_CHANNELS_11_P];
	u8 pwr_limit_reference_11p;
	u8 per_sub_band_tx_trace_loss[WL18XX_NUM_OF_SUB_BANDS];
	u8 per_sub_band_rx_trace_loss[WL18XX_NUM_OF_SUB_BANDS];
	u8 primary_clock_setting_time;
	u8 clock_valid_on_wake_up;
	u8 secondary_clock_setting_time;
	u8 board_type;
	/* enable point saturation */
	u8 psat;
	/* low/medium/high Tx power in dBm */
	s8 low_power_val;
	s8 med_power_val;
	s8 high_power_val;
	u8 padding[1];
} __packed;

struct wl18xx_priv_conf {
	/* this structure is copied wholesale to FW */
	struct wl18xx_mac_and_phy_params phy;
} __packed;

#endif /* __WL18XX_CONF_H__ */
