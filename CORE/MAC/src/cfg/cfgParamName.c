/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * DO NOT EDIT - This file is generated automatically
 */

/*
 * IMPORTANT:  This file is for system that supports STA mode ONLY.
 */
#include "cfgPriv.h"

unsigned char *gCfgParamName[] = {
    (unsigned char *)"STA_ID",
    (unsigned char *)"CF_POLLABLE",
    (unsigned char *)"CFP_PERIOD",
    (unsigned char *)"CFP_MAX_DURATION",
    (unsigned char *)"SSID",
    (unsigned char *)"BEACON_INTERVAL",
    (unsigned char *)"DTIM_PERIOD",
    (unsigned char *)"WEP_KEY_LENGTH",
    (unsigned char *)"WEP_DEFAULT_KEY_1",
    (unsigned char *)"WEP_DEFAULT_KEY_2",
    (unsigned char *)"WEP_DEFAULT_KEY_3",
    (unsigned char *)"WEP_DEFAULT_KEY_4",
    (unsigned char *)"WEP_DEFAULT_KEYID",
    (unsigned char *)"EXCLUDE_UNENCRYPTED",
    (unsigned char *)"RTS_THRESHOLD",
    (unsigned char *)"SHORT_RETRY_LIMIT",
    (unsigned char *)"LONG_RETRY_LIMIT",
    (unsigned char *)"FRAGMENTATION_THRESHOLD",
    (unsigned char *)"ACTIVE_MINIMUM_CHANNEL_TIME",
    (unsigned char *)"ACTIVE_MAXIMUM_CHANNEL_TIME",
    (unsigned char *)"PASSIVE_MINIMUM_CHANNEL_TIME",
    (unsigned char *)"PASSIVE_MAXIMUM_CHANNEL_TIME",
    (unsigned char *)"JOIN_FAILURE_TIMEOUT",
    (unsigned char *)"AUTHENTICATE_FAILURE_TIMEOUT",
    (unsigned char *)"AUTHENTICATE_RSP_TIMEOUT",
    (unsigned char *)"ASSOCIATION_FAILURE_TIMEOUT",
    (unsigned char *)"REASSOCIATION_FAILURE_TIMEOUT",
    (unsigned char *)"RA_PERIODICITY_TIMEOUT_IN_PS",
    (unsigned char *)"PS_ENABLE_BCN_FILTER",
    (unsigned char *)"PS_ENABLE_HEART_BEAT",
    (unsigned char *)"PS_ENABLE_RSSI_MONITOR",
    (unsigned char *)"PS_DATA_INACTIVITY_TIMEOUT",
    (unsigned char *)"RF_SETTLING_TIME_CLK",
    (unsigned char *)"SUPPORTED_RATES_11B",
    (unsigned char *)"SUPPORTED_RATES_11A",
    (unsigned char *)"PHY_MODE",
    (unsigned char *)"DOT11_MODE",
    (unsigned char *)"OPERATIONAL_RATE_SET",
    (unsigned char *)"EXTENDED_OPERATIONAL_RATE_SET",
    (unsigned char *)"PROPRIETARY_OPERATIONAL_RATE_SET",
    (unsigned char *)"BSSID",
    (unsigned char *)"LISTEN_INTERVAL",
    (unsigned char *)"VALID_CHANNEL_LIST",
    (unsigned char *)"CURRENT_CHANNEL",
    (unsigned char *)"DEFAULT_RATE_INDEX_5GHZ",
    (unsigned char *)"DEFAULT_RATE_INDEX_24GHZ",
    (unsigned char *)"RATE_ADAPTATION_TYPE",
    (unsigned char *)"FIXED_RATE",
    (unsigned char *)"FIXED_RATE_MULTICAST_24GHZ",
    (unsigned char *)"FIXED_RATE_MULTICAST_5GHZ",
    (unsigned char *)"RETRYRATE_POLICY",
    (unsigned char *)"RETRYRATE_SECONDARY",
    (unsigned char *)"RETRYRATE_TERTIARY",
    (unsigned char *)"APSD_ENABLED",
    (unsigned char *)"SHARED_KEY_AUTH_ENABLE",
    (unsigned char *)"OPEN_SYSTEM_AUTH_ENABLE",
    (unsigned char *)"AUTHENTICATION_TYPE",
    (unsigned char *)"CF_POLL_REQUEST",
    (unsigned char *)"PRIVACY_ENABLED",
    (unsigned char *)"SHORT_PREAMBLE",
    (unsigned char *)"SHORT_SLOT_TIME",
    (unsigned char *)"ACCEPT_SHORT_SLOT_ASSOC_ONLY",
    (unsigned char *)"QOS_ENABLED",
    (unsigned char *)"HCF_ENABLED",
    (unsigned char *)"RSN_ENABLED",
    (unsigned char *)"BACKGROUND_SCAN_PERIOD",
    (unsigned char *)"MAX_NUM_PRE_AUTH",
    (unsigned char *)"PREAUTH_CLNUP_TIMEOUT",
    (unsigned char *)"RELEASE_AID_TIMEOUT",
    (unsigned char *)"HEART_BEAT_THRESHOLD",
    (unsigned char *)"PROBE_AFTER_HB_FAIL_TIMEOUT",
    (unsigned char *)"MANUFACTURER_OUI",
    (unsigned char *)"MANUFACTURER_NAME",
    (unsigned char *)"MODEL_NUMBER",
    (unsigned char *)"MODEL_NAME",
    (unsigned char *)"MANUFACTURER_PRODUCT_NAME",
    (unsigned char *)"MANUFACTURER_PRODUCT_VERSION",
    (unsigned char *)"11D_ENABLED",
    (unsigned char *)"MAX_TX_POWER_2_4",
    (unsigned char *)"MAX_TX_POWER_5",
    (unsigned char *)"NETWORK_DENSITY",
    (unsigned char *)"ADAPTIVE_THRESHOLD_ALGORITHM",
    (unsigned char *)"CURRENT_TX_ANTENNA",
    (unsigned char *)"CURRENT_RX_ANTENNA",
    (unsigned char *)"CURRENT_TX_POWER_LEVEL",
    (unsigned char *)"POWER_STATE_PER_CHAIN",
    (unsigned char *)"NEW_BSS_FOUND_IND",
    (unsigned char *)"PROPRIETARY_ANI_FEATURES_ENABLED",
    (unsigned char *)"PROPRIETARY_RATES_ENABLED",
    (unsigned char *)"AP_NODE_NAME",
    (unsigned char *)"COUNTRY_CODE",
    (unsigned char *)"11H_ENABLED",
    (unsigned char *)"WT_CNF_TIMEOUT",
    (unsigned char *)"KEEPALIVE_TIMEOUT",
    (unsigned char *)"PROXIMITY",
    (unsigned char *)"LOG_LEVEL",
    (unsigned char *)"OLBC_DETECT_TIMEOUT",
    (unsigned char *)"PROTECTION_ENABLED",
    (unsigned char *)"11G_PROTECTION_ALWAYS",
    (unsigned char *)"FORCE_POLICY_PROTECTION",
    (unsigned char *)"11G_SHORT_PREAMBLE_ENABLED",
    (unsigned char *)"11G_SHORT_SLOT_TIME_ENABLED",
    (unsigned char *)"CAL_PERIOD",
    (unsigned char *)"STATS_PERIOD",
    (unsigned char *)"CAL_CONTROL",
    (unsigned char *)"11G_ONLY_POLICY",
    (unsigned char *)"PACKET_CLASSIFICATION",
    (unsigned char *)"WME_ENABLED",
    (unsigned char *)"ADDTS_RSP_TIMEOUT",
    (unsigned char *)"MAX_SP_LENGTH",
    (unsigned char *)"KEEP_ALIVE_STA_LIMIT_THRESHOLD",
    (unsigned char *)"SEND_SINGLE_SSID_ALWAYS",
    (unsigned char *)"WSM_ENABLED",
    (unsigned char *)"PROP_CAPABILITY",
    (unsigned char *)"EDCA_PROFILE",
    (unsigned char *)"EDCA_ANI_ACBK_LOCAL",
    (unsigned char *)"EDCA_ANI_ACBE_LOCAL",
    (unsigned char *)"EDCA_ANI_ACVI_LOCAL",
    (unsigned char *)"EDCA_ANI_ACVO_LOCAL",
    (unsigned char *)"EDCA_ANI_ACBK",
    (unsigned char *)"EDCA_ANI_ACBE",
    (unsigned char *)"EDCA_ANI_ACVI",
    (unsigned char *)"EDCA_ANI_ACVO",
    (unsigned char *)"EDCA_WME_ACBK_LOCAL",
    (unsigned char *)"EDCA_WME_ACBE_LOCAL",
    (unsigned char *)"EDCA_WME_ACVI_LOCAL",
    (unsigned char *)"EDCA_WME_ACVO_LOCAL",
    (unsigned char *)"EDCA_WME_ACBK",
    (unsigned char *)"EDCA_WME_ACBE",
    (unsigned char *)"EDCA_WME_ACVI",
    (unsigned char *)"EDCA_WME_ACVO",
    (unsigned char *)"EDCA_TIT_DEMO_ACBK_LOCAL",
    (unsigned char *)"EDCA_TIT_DEMO_ACBE_LOCAL",
    (unsigned char *)"EDCA_TIT_DEMO_ACVI_LOCAL",
    (unsigned char *)"EDCA_TIT_DEMO_ACVO_LOCAL",
    (unsigned char *)"EDCA_TIT_DEMO_ACBK",
    (unsigned char *)"EDCA_TIT_DEMO_ACBE",
    (unsigned char *)"EDCA_TIT_DEMO_ACVI",
    (unsigned char *)"EDCA_TIT_DEMO_ACVO",
    (unsigned char *)"RDET_FLAG",
    (unsigned char *)"RADAR_CHANNEL_LIST",
    (unsigned char *)"LOCAL_POWER_CONSTRAINT",
    (unsigned char *)"ADMIT_POLICY",
    (unsigned char *)"ADMIT_BWFACTOR",
    (unsigned char *)"MAX_CONSECUTIVE_BACKGROUND_SCAN_FAILURE",
    (unsigned char *)"CHANNEL_BONDING_MODE",
    (unsigned char *)"CB_SECONDARY_CHANNEL_STATE",
    (unsigned char *)"DYNAMIC_THRESHOLD_ZERO",
    (unsigned char *)"DYNAMIC_THRESHOLD_ONE",
    (unsigned char *)"DYNAMIC_THRESHOLD_TWO",
    (unsigned char *)"TRIG_STA_BK_SCAN",
    (unsigned char *)"DYNAMIC_PROFILE_SWITCHING",
    (unsigned char *)"SCAN_CONTROL_LIST",
    (unsigned char *)"MIMO_ENABLED",
    (unsigned char *)"BLOCK_ACK_ENABLED",
    (unsigned char *)"BA_ACTIVITY_CHECK_TIMEOUT",
    (unsigned char *)"HT_RX_STBC",
    (unsigned char *)"HT_CAP_INFO",
    (unsigned char *)"HT_AMPDU_PARAMS",
    (unsigned char *)"SUPPORTED_MCS_SET",
    (unsigned char *)"EXT_HT_CAP_INFO",
    (unsigned char *)"TX_BF_CAP",
    (unsigned char *)"AS_CAP",
    (unsigned char *)"HT_INFO_FIELD1",
    (unsigned char *)"HT_INFO_FIELD2",
    (unsigned char *)"HT_INFO_FIELD3",
    (unsigned char *)"BASIC_MCS_SET",
    (unsigned char *)"CURRENT_MCS_SET",
    (unsigned char *)"GREENFIELD_CAPABILITY",
    (unsigned char *)"VHT_MAX_MPDU_LENGTH",
    (unsigned char *)"VHT_SUPPORTED_CHAN_WIDTH_SET",
    (unsigned char *)"VHT_LDPC_CODING_CAP",
    (unsigned char *)"VHT_SHORT_GI_80MHZ",
    (unsigned char *)"VHT_SHORT_GI_160_AND_80_PLUS_80MHZ",
    (unsigned char *)"VHT_TXSTBC",
    (unsigned char *)"VHT_RXSTBC",
    (unsigned char *)"VHT_SU_BEAMFORMER_CAP",
    (unsigned char *)"VHT_SU_BEAMFORMEE_CAP",
    (unsigned char *)"VHT_CSN_BEAMFORMEE_ANT_SUPPORTED",
    (unsigned char *)"VHT_NUM_SOUNDING_DIMENSIONS",
    (unsigned char *)"VHT_MU_BEAMFORMER_CAP",
    (unsigned char *)"VHT_MU_BEAMFORMEE_CAP",
    (unsigned char *)"VHT_TXOP_PS",
    (unsigned char *)"VHT_HTC_VHTC_CAP",
    (unsigned char *)"VHT_AMPDU_LEN_EXPONENT",
    (unsigned char *)"VHT_LINK_ADAPTATION_CAP",
    (unsigned char *)"VHT_RX_ANT_PATTERN",
    (unsigned char *)"VHT_TX_ANT_PATTERN",
    (unsigned char *)"VHT_RX_MCS_MAP",
    (unsigned char *)"VHT_TX_MCS_MAP",
    (unsigned char *)"VHT_RX_HIGHEST_SUPPORTED_DATA_RATE",
    (unsigned char *)"VHT_TX_HIGHEST_SUPPORTED_DATA_RATE",
    (unsigned char *)"VHT_CHANNEL_WIDTH",
    (unsigned char *)"VHT_CHANNEL_CENTER_FREQ_SEGMENT1",
    (unsigned char *)"VHT_CHANNEL_CENTER_FREQ_SEGMENT2",
    (unsigned char *)"VHT_BASIC_MCS_SET",
    (unsigned char *)"VHT_MU_MIMO_CAP_STA_COUNT",
    (unsigned char *)"VHT_SS_UNDER_UTIL",
    (unsigned char *)"VHT_40MHZ_UTILIZATION",
    (unsigned char *)"VHT_80MHZ_UTILIZATION",
    (unsigned char *)"VHT_160MHZ_UTILIZATION",
    (unsigned char *)"MAX_AMSDU_LENGTH",
    (unsigned char *)"MPDU_DENSITY",
    (unsigned char *)"NUM_BUFF_ADVERT",
    (unsigned char *)"MAX_RX_AMPDU_FACTOR",
    (unsigned char *)"SHORT_GI_20MHZ",
    (unsigned char *)"SHORT_GI_40MHZ",
    (unsigned char *)"RIFS_ENABLED",
    (unsigned char *)"MAX_PS_POLL",
    (unsigned char *)"NUM_BEACON_PER_RSSI_AVERAGE",
    (unsigned char *)"RSSI_FILTER_PERIOD",
    (unsigned char *)"MIN_RSSI_THRESHOLD",
    (unsigned char *)"NTH_BEACON_FILTER",
    (unsigned char *)"BROADCAST_FRAME_FILTER_ENABLE",
    (unsigned char *)"SCAN_IN_POWERSAVE",
    (unsigned char *)"IGNORE_DTIM",
    (unsigned char *)"WOWLAN_UCAST_PATTERN_FILTER_ENABLE",
    (unsigned char *)"WOWLAN_CHANNEL_SWITCH_ENABLE",
    (unsigned char *)"WOWLAN_DEAUTH_ENABLE",
    (unsigned char *)"WOWLAN_DISASSOC_ENABLE",
    (unsigned char *)"WOWLAN_MAX_MISSED_BEACON",
    (unsigned char *)"WOWLAN_MAX_SLEEP_PERIOD",
    (unsigned char *)"BA_TIMEOUT",
    (unsigned char *)"BA_THRESHOLD_HIGH",
    (unsigned char *)"MAX_BA_BUFFERS",
    (unsigned char *)"MAX_BA_SESSIONS",
    (unsigned char *)"BA_AUTO_SETUP",
    (unsigned char *)"ADDBA_REQ_DECLINE",
    (unsigned char *)"BG_SCAN_CHANNEL_LIST",
    (unsigned char *)"MAX_MEDIUM_TIME",
    (unsigned char *)"MAX_MPDUS_IN_AMPDU",
    (unsigned char *)"IBSS_AUTO_BSSID",
    (unsigned char *)"PROBE_REQ_ADDNIE_FLAG",
    (unsigned char *)"PROBE_REQ_ADDNIE_DATA",
    (unsigned char *)"PROBE_RSP_ADDNIE_FLAG",
    (unsigned char *)"PROBE_RSP_ADDNIE_DATA1",
    (unsigned char *)"PROBE_RSP_ADDNIE_DATA2",
    (unsigned char *)"PROBE_RSP_ADDNIE_DATA3",
    (unsigned char *)"ASSOC_RSP_ADDNIE_FLAG",
    (unsigned char *)"ASSOC_RSP_ADDNIE_DATA",
    (unsigned char *)"PROBE_REQ_ADDNP2PIE_FLAG",
    (unsigned char *)"PROBE_REQ_ADDNP2PIE_DATA",
    (unsigned char *)"PROBE_RSP_BCN_ADDNIE_FLAG",
    (unsigned char *)"PROBE_RSP_BCN_ADDNIE_DATA",
    (unsigned char *)"WPS_ENABLE",
    (unsigned char *)"WPS_STATE",
    (unsigned char *)"WPS_PROBE_REQ_FLAG",
    (unsigned char *)"WPS_VERSION",
    (unsigned char *)"WPS_REQUEST_TYPE",
    (unsigned char *)"WPS_CFG_METHOD",
    (unsigned char *)"WPS_UUID",
    (unsigned char *)"WPS_PRIMARY_DEVICE_CATEGORY",
    (unsigned char *)"WPS_PIMARY_DEVICE_OUI",
    (unsigned char *)"WPS_DEVICE_SUB_CATEGORY",
    (unsigned char *)"WPS_ASSOCIATION_STATE",
    (unsigned char *)"WPS_CONFIGURATION_ERROR",
    (unsigned char *)"WPS_DEVICE_PASSWORD_ID",
    (unsigned char *)"WPS_ASSOC_METHOD",
    (unsigned char *)"LOW_GAIN_OVERRIDE",
    (unsigned char *)"ENABLE_PHY_AGC_LISTEN_MODE",
    (unsigned char *)"RPE_POLLING_THRESHOLD",
    (unsigned char *)"RPE_AGING_THRESHOLD_FOR_AC0_REG",
    (unsigned char *)"RPE_AGING_THRESHOLD_FOR_AC1_REG",
    (unsigned char *)"RPE_AGING_THRESHOLD_FOR_AC2_REG",
    (unsigned char *)"RPE_AGING_THRESHOLD_FOR_AC3_REG",
    (unsigned char *)"NO_OF_ONCHIP_REORDER_SESSIONS",
    (unsigned char *)"SINGLE_TID_RC",
    (unsigned char *)"RRM_ENABLED",
    (unsigned char *)"RRM_OPERATING_CHAN_MAX",
    (unsigned char *)"RRM_NON_OPERATING_CHAN_MAX",
    (unsigned char *)"TX_PWR_CTRL_ENABLE",
    (unsigned char *)"MCAST_BCAST_FILTER_SETTING",
    (unsigned char *)"BTC_DHCP_BT_SLOTS_TO_BLOCK",
    (unsigned char *)"DYNAMIC_PS_POLL_VALUE",
    (unsigned char *)"PS_NULLDATA_AP_RESP_TIMEOUT",
    (unsigned char *)"TELE_BCN_WAKEUP_EN",
    (unsigned char *)"TELE_BCN_TRANS_LI",
    (unsigned char *)"TELE_BCN_TRANS_LI_IDLE_BCNS",
    (unsigned char *)"TELE_BCN_MAX_LI",
    (unsigned char *)"TELE_BCN_MAX_LI_IDLE_BCNS",
    (unsigned char *)"BTC_A2DP_DHCP_BT_SUB_INTERVALS",
    (unsigned char *)"INFRA_STA_KEEP_ALIVE_PERIOD",
    (unsigned char *)"ASSOC_STA_LIMIT",
    (unsigned char *)"SAP_CHANNEL_SELECT_START_CHANNEL",
    (unsigned char *)"SAP_CHANNEL_SELECT_END_CHANNEL",
    (unsigned char *)"SAP_CHANNEL_SELECT_OPERATING_BAND",
    (unsigned char *)"AP_DATA_AVAIL_POLL_PERIOD",
    (unsigned char *)"ENABLE_CLOSE_LOOP",
    (unsigned char *)"ENABLE_LTE_COEX",
    (unsigned char *)"AP_KEEP_ALIVE_TIMEOUT",
    (unsigned char *)"GO_KEEP_ALIVE_TIMEOUT",
    (unsigned char *)"ENABLE_MC_ADDR_LIST",
    (unsigned char *)"ENABLE_UC_FILTER",
    (unsigned char *)"ENABLE_LPWR_IMG_TRANSITION",
    (unsigned char *)"ENABLE_MCC_ADAPTIVE_SCHED",
    (unsigned char *)"DISABLE_LDPC_WITH_TXBF_AP",
    (unsigned char *)"AP_LINK_MONITOR_TIMEOUT",
};



