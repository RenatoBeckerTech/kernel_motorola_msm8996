/*
 *  linux/drivers/mmc/core/mmc.c
 *
 *  Copyright (C) 2003-2004 Russell King, All Rights Reserved.
 *  Copyright (C) 2005-2007 Pierre Ossman, All Rights Reserved.
 *  MMCv4 support Copyright (C) 2006 Philip Langdale, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/pm_runtime.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/reboot.h>
#include <trace/events/mmc.h>

#include "core.h"
#include "bus.h"
#include "mmc_ops.h"
#include "sd_ops.h"

static const unsigned int tran_exp[] = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

static const unsigned char tran_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int tacc_exp[] = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

static const unsigned int tacc_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

/*
 * Given the decoded CSD structure, decode the raw CID to our CID structure.
 */
static int mmc_decode_cid(struct mmc_card *card)
{
	u32 *resp = card->raw_cid;

	/*
	 * The selection of the format here is based upon published
	 * specs from sandisk and from what people have reported.
	 */
	switch (card->csd.mmca_vsn) {
	case 0: /* MMC v1.0 - v1.2 */
	case 1: /* MMC v1.4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 104, 24);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prod_name[6]	= UNSTUFF_BITS(resp, 48, 8);
		card->cid.hwrev		= UNSTUFF_BITS(resp, 44, 4);
		card->cid.fwrev		= UNSTUFF_BITS(resp, 40, 4);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 24);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	case 2: /* MMC v2.0 - v2.2 */
	case 3: /* MMC v3.1 - v3.3 */
	case 4: /* MMC v4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 120, 8);
		card->cid.oemid		= UNSTUFF_BITS(resp, 104, 16);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prv		= UNSTUFF_BITS(resp, 48, 8);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 32);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	default:
		pr_err("%s: card has unknown MMCA version %d\n",
			mmc_hostname(card->host), card->csd.mmca_vsn);
		return -EINVAL;
	}

	return 0;
}

static void mmc_set_erase_size(struct mmc_card *card)
{
	if (card->ext_csd.erase_group_def & 1)
		card->erase_size = card->ext_csd.hc_erase_size;
	else
		card->erase_size = card->csd.erase_size;

	mmc_init_erase(card);
}

static const struct mmc_fixup mmc_fixups[] = {

	/* avoid HPI for specific cards */
	MMC_FIXUP_EXT_CSD_REV("MMC16G", CID_MANFID_KINGSTON, CID_OEMID_ANY,
		add_quirk, MMC_QUIRK_BROKEN_HPI, MMC_V4_41),

	/* Disable cache for specific cards */
	MMC_FIXUP("MMC16G", CID_MANFID_KINGSTON, CID_OEMID_ANY,
		add_quirk_mmc, MMC_QUIRK_CACHE_DISABLE),

	END_FIXUP
};

/*
 * Given a 128-bit response, decode to our card CSD structure.
 */
static int mmc_decode_csd(struct mmc_card *card)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m, a, b;
	u32 *resp = card->raw_csd;

	/*
	 * We only understand CSD structure v1.1 and v1.2.
	 * v1.2 has extra information in bits 15, 11 and 10.
	 * We also support eMMC v4.4 & v4.41.
	 */
	csd->structure = UNSTUFF_BITS(resp, 126, 2);
	if (csd->structure == 0) {
		pr_err("%s: unrecognised CSD structure version %d\n",
			mmc_hostname(card->host), csd->structure);
		return -EINVAL;
	}

	csd->mmca_vsn	 = UNSTUFF_BITS(resp, 122, 4);
	m = UNSTUFF_BITS(resp, 115, 4);
	e = UNSTUFF_BITS(resp, 112, 3);
	csd->tacc_ns	 = (tacc_exp[e] * tacc_mant[m] + 9) / 10;
	csd->tacc_clks	 = UNSTUFF_BITS(resp, 104, 8) * 100;

	m = UNSTUFF_BITS(resp, 99, 4);
	e = UNSTUFF_BITS(resp, 96, 3);
	csd->max_dtr	  = tran_exp[e] * tran_mant[m];
	csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);

	e = UNSTUFF_BITS(resp, 47, 3);
	m = UNSTUFF_BITS(resp, 62, 12);
	csd->capacity	  = (1 + m) << (e + 2);

	csd->read_blkbits = UNSTUFF_BITS(resp, 80, 4);
	csd->read_partial = UNSTUFF_BITS(resp, 79, 1);
	csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
	csd->read_misalign = UNSTUFF_BITS(resp, 77, 1);
	csd->dsr_imp = UNSTUFF_BITS(resp, 76, 1);
	csd->r2w_factor = UNSTUFF_BITS(resp, 26, 3);
	csd->write_blkbits = UNSTUFF_BITS(resp, 22, 4);
	csd->write_partial = UNSTUFF_BITS(resp, 21, 1);

	if (csd->write_blkbits >= 9) {
		a = UNSTUFF_BITS(resp, 42, 5);
		b = UNSTUFF_BITS(resp, 37, 5);
		csd->erase_size = (a + 1) * (b + 1);
		csd->erase_size <<= csd->write_blkbits - 9;
	}

	return 0;
}

/*
 * Read extended CSD.
 */
static int mmc_get_ext_csd(struct mmc_card *card, u8 **new_ext_csd)
{
	int err;
	u8 *ext_csd;

	BUG_ON(!card);
	BUG_ON(!new_ext_csd);

	*new_ext_csd = NULL;

	if (!mmc_can_ext_csd(card))
		return 0;

	/*
	 * As the ext_csd is so large and mostly unused, we don't store the
	 * raw block in mmc_card.
	 */
	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		pr_err("%s: could not allocate a buffer to "
			"receive the ext_csd.\n", mmc_hostname(card->host));
		return -ENOMEM;
	}

	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		kfree(ext_csd);
		*new_ext_csd = NULL;

		/* If the host or the card can't do the switch,
		 * fail more gracefully. */
		if ((err != -EINVAL)
		 && (err != -ENOSYS)
		 && (err != -EFAULT))
			return err;

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			pr_err("%s: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			pr_warn("%s: unable to read EXT_CSD, performance might suffer\n",
				mmc_hostname(card->host));
			err = 0;
		}
	} else
		*new_ext_csd = ext_csd;

	return err;
}

static void mmc_select_card_type(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u8 card_type = card->ext_csd.raw_card_type;
	u32 caps = host->caps, caps2 = host->caps2;
	unsigned int hs_max_dtr = 0, hs200_max_dtr = 0;
	unsigned int avail_type = 0;

	if (caps & MMC_CAP_MMC_HIGHSPEED &&
	    card_type & EXT_CSD_CARD_TYPE_HS_26) {
		hs_max_dtr = MMC_HIGH_26_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS_26;
	}

	if (caps & MMC_CAP_MMC_HIGHSPEED &&
	    card_type & EXT_CSD_CARD_TYPE_HS_52) {
		hs_max_dtr = MMC_HIGH_52_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS_52;
	}

	if (caps & MMC_CAP_1_8V_DDR &&
	    card_type & EXT_CSD_CARD_TYPE_DDR_1_8V) {
		hs_max_dtr = MMC_HIGH_DDR_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_DDR_1_8V;
	}

	if (caps & MMC_CAP_1_2V_DDR &&
	    card_type & EXT_CSD_CARD_TYPE_DDR_1_2V) {
		hs_max_dtr = MMC_HIGH_DDR_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_DDR_1_2V;
	}

	if (caps2 & MMC_CAP2_HS200_1_8V_SDR &&
	    card_type & EXT_CSD_CARD_TYPE_HS200_1_8V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS200_1_8V;
	}

	if (caps2 & MMC_CAP2_HS200_1_2V_SDR &&
	    card_type & EXT_CSD_CARD_TYPE_HS200_1_2V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS200_1_2V;
	}

	if (caps2 & MMC_CAP2_HS400_1_8V &&
	    card_type & EXT_CSD_CARD_TYPE_HS400_1_8V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS400_1_8V;
	}

	if (caps2 & MMC_CAP2_HS400_1_2V &&
	    card_type & EXT_CSD_CARD_TYPE_HS400_1_2V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS400_1_2V;
	}

	card->ext_csd.hs_max_dtr = hs_max_dtr;
	card->ext_csd.hs200_max_dtr = hs200_max_dtr;
	card->mmc_avail_type = avail_type;
}

static void mmc_manage_enhanced_area(struct mmc_card *card, u8 *ext_csd)
{
	u8 hc_erase_grp_sz, hc_wp_grp_sz;

	/*
	 * Disable these attributes by default
	 */
	card->ext_csd.enhanced_area_offset = -EINVAL;
	card->ext_csd.enhanced_area_size = -EINVAL;

	/*
	 * Enhanced area feature support -- check whether the eMMC
	 * card has the Enhanced area enabled.  If so, export enhanced
	 * area offset and size to user by adding sysfs interface.
	 */
	if ((ext_csd[EXT_CSD_PARTITION_SUPPORT] & 0x2) &&
	    (ext_csd[EXT_CSD_PARTITION_ATTRIBUTE] & 0x1)) {
		if (card->ext_csd.partition_setting_completed) {
			hc_erase_grp_sz =
				ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
			hc_wp_grp_sz =
				ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

			/*
			 * calculate the enhanced data area offset, in bytes
			 */
			card->ext_csd.enhanced_area_offset =
				(ext_csd[139] << 24) + (ext_csd[138] << 16) +
				(ext_csd[137] << 8) + ext_csd[136];
			if (mmc_card_blockaddr(card))
				card->ext_csd.enhanced_area_offset <<= 9;
			/*
			 * calculate the enhanced data area size, in kilobytes
			 */
			card->ext_csd.enhanced_area_size =
				(ext_csd[142] << 16) + (ext_csd[141] << 8) +
				ext_csd[140];
			card->ext_csd.enhanced_area_size *=
				(size_t)(hc_erase_grp_sz * hc_wp_grp_sz);
			card->ext_csd.enhanced_area_size <<= 9;
		} else {
			pr_warn("%s: defines enhanced area without partition setting complete\n",
				mmc_hostname(card->host));
		}
	}
}

static void mmc_manage_gp_partitions(struct mmc_card *card, u8 *ext_csd)
{
	int idx;
	u8 hc_erase_grp_sz, hc_wp_grp_sz;
	unsigned int part_size;

	/*
	 * General purpose partition feature support --
	 * If ext_csd has the size of general purpose partitions,
	 * set size, part_cfg, partition name in mmc_part.
	 */
	if (ext_csd[EXT_CSD_PARTITION_SUPPORT] &
	    EXT_CSD_PART_SUPPORT_PART_EN) {
		hc_erase_grp_sz =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
		hc_wp_grp_sz =
			ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

		for (idx = 0; idx < MMC_NUM_GP_PARTITION; idx++) {
			if (!ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3] &&
			    !ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 1] &&
			    !ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 2])
				continue;
			if (card->ext_csd.partition_setting_completed == 0) {
				pr_warn("%s: has partition size defined without partition complete\n",
					mmc_hostname(card->host));
				break;
			}
			part_size =
				(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 2]
				<< 16) +
				(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 1]
				<< 8) +
				ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3];
			part_size *= (size_t)(hc_erase_grp_sz *
				hc_wp_grp_sz);
			mmc_part_add(card, part_size << 19,
				EXT_CSD_PART_CONFIG_ACC_GP0 + idx,
				"gp%d", idx, false,
				MMC_BLK_DATA_AREA_GP);
		}
	}
}

/*
 * Decode extended CSD.
 */
static int mmc_read_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	int err = 0, idx;
	unsigned int part_size;

	BUG_ON(!card);

	if (!ext_csd)
		return 0;

	/* Version is coded in the CSD_STRUCTURE byte in the EXT_CSD register */
	card->ext_csd.raw_ext_csd_structure = ext_csd[EXT_CSD_STRUCTURE];
	if (card->csd.structure == 3) {
		if (card->ext_csd.raw_ext_csd_structure > 2) {
			pr_err("%s: unrecognised EXT_CSD structure "
				"version %d\n", mmc_hostname(card->host),
					card->ext_csd.raw_ext_csd_structure);
			err = -EINVAL;
			goto out;
		}
	}

	/*
	 * The EXT_CSD format is meant to be forward compatible. As long
	 * as CSD_STRUCTURE does not change, all values for EXT_CSD_REV
	 * are authorized, see JEDEC JESD84-B50 section B.8.
	 */
	card->ext_csd.rev = ext_csd[EXT_CSD_REV];

	card->ext_csd.raw_sectors[0] = ext_csd[EXT_CSD_SEC_CNT + 0];
	card->ext_csd.raw_sectors[1] = ext_csd[EXT_CSD_SEC_CNT + 1];
	card->ext_csd.raw_sectors[2] = ext_csd[EXT_CSD_SEC_CNT + 2];
	card->ext_csd.raw_sectors[3] = ext_csd[EXT_CSD_SEC_CNT + 3];
	if (card->ext_csd.rev >= 2) {
		card->ext_csd.sectors =
			ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
			ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
			ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
			ext_csd[EXT_CSD_SEC_CNT + 3] << 24;

		/* Cards with density > 2GiB are sector addressed */
		if (card->ext_csd.sectors > (2u * 1024 * 1024 * 1024) / 512)
			mmc_card_set_blockaddr(card);
	}

	card->ext_csd.raw_card_type = ext_csd[EXT_CSD_CARD_TYPE];
	mmc_select_card_type(card);

	card->ext_csd.raw_drive_strength = ext_csd[EXT_CSD_DRIVE_STRENGTH];

	card->ext_csd.raw_s_a_timeout = ext_csd[EXT_CSD_S_A_TIMEOUT];
	card->ext_csd.raw_erase_timeout_mult =
		ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
	card->ext_csd.raw_hc_erase_grp_size =
		ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
	if (card->ext_csd.rev >= 3) {
		u8 sa_shift = ext_csd[EXT_CSD_S_A_TIMEOUT];
		card->ext_csd.part_config = ext_csd[EXT_CSD_PART_CONFIG];

		/* EXT_CSD value is in units of 10ms, but we store in ms */
		card->ext_csd.part_time = 10 * ext_csd[EXT_CSD_PART_SWITCH_TIME];

		/* Sleep / awake timeout in 100ns units */
		if (sa_shift > 0 && sa_shift <= 0x17)
			card->ext_csd.sa_timeout =
					1 << ext_csd[EXT_CSD_S_A_TIMEOUT];
		card->ext_csd.erase_group_def =
			ext_csd[EXT_CSD_ERASE_GROUP_DEF];
		card->ext_csd.hc_erase_timeout = 300 *
			ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
		card->ext_csd.hc_erase_size =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] << 10;

		card->ext_csd.rel_sectors = ext_csd[EXT_CSD_REL_WR_SEC_C];

		/*
		 * There are two boot regions of equal size, defined in
		 * multiples of 128K.
		 */
		if (ext_csd[EXT_CSD_BOOT_MULT] && mmc_boot_partition_access(card->host)) {
			for (idx = 0; idx < MMC_NUM_BOOT_PARTITION; idx++) {
				part_size = ext_csd[EXT_CSD_BOOT_MULT] << 17;
				mmc_part_add(card, part_size,
					EXT_CSD_PART_CONFIG_ACC_BOOT0 + idx,
					"boot%d", idx, true,
					MMC_BLK_DATA_AREA_BOOT);
			}
		}
	}

	card->ext_csd.raw_hc_erase_gap_size =
		ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
	card->ext_csd.raw_sec_trim_mult =
		ext_csd[EXT_CSD_SEC_TRIM_MULT];
	card->ext_csd.raw_sec_erase_mult =
		ext_csd[EXT_CSD_SEC_ERASE_MULT];
	card->ext_csd.raw_sec_feature_support =
		ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
	card->ext_csd.raw_trim_mult =
		ext_csd[EXT_CSD_TRIM_MULT];
	card->ext_csd.raw_partition_support = ext_csd[EXT_CSD_PARTITION_SUPPORT];
	if (card->ext_csd.rev >= 4) {
		if (ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED] &
		    EXT_CSD_PART_SETTING_COMPLETED)
			card->ext_csd.partition_setting_completed = 1;
		else
			card->ext_csd.partition_setting_completed = 0;

		mmc_manage_enhanced_area(card, ext_csd);

		mmc_manage_gp_partitions(card, ext_csd);

		card->ext_csd.sec_trim_mult =
			ext_csd[EXT_CSD_SEC_TRIM_MULT];
		card->ext_csd.sec_erase_mult =
			ext_csd[EXT_CSD_SEC_ERASE_MULT];
		card->ext_csd.sec_feature_support =
			ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
		card->ext_csd.trim_timeout = 300 *
			ext_csd[EXT_CSD_TRIM_MULT];

		/*
		 * Note that the call to mmc_part_add above defaults to read
		 * only. If this default assumption is changed, the call must
		 * take into account the value of boot_locked below.
		 */
		card->ext_csd.boot_ro_lock = ext_csd[EXT_CSD_BOOT_WP];
		card->ext_csd.boot_ro_lockable = true;

		/* Save power class values */
		card->ext_csd.raw_pwr_cl_52_195 =
			ext_csd[EXT_CSD_PWR_CL_52_195];
		card->ext_csd.raw_pwr_cl_26_195 =
			ext_csd[EXT_CSD_PWR_CL_26_195];
		card->ext_csd.raw_pwr_cl_52_360 =
			ext_csd[EXT_CSD_PWR_CL_52_360];
		card->ext_csd.raw_pwr_cl_26_360 =
			ext_csd[EXT_CSD_PWR_CL_26_360];
		card->ext_csd.raw_pwr_cl_200_195 =
			ext_csd[EXT_CSD_PWR_CL_200_195];
		card->ext_csd.raw_pwr_cl_200_360 =
			ext_csd[EXT_CSD_PWR_CL_200_360];
		card->ext_csd.raw_pwr_cl_ddr_52_195 =
			ext_csd[EXT_CSD_PWR_CL_DDR_52_195];
		card->ext_csd.raw_pwr_cl_ddr_52_360 =
			ext_csd[EXT_CSD_PWR_CL_DDR_52_360];
		card->ext_csd.raw_pwr_cl_ddr_200_360 =
			ext_csd[EXT_CSD_PWR_CL_DDR_200_360];
	}

	/* check whether the eMMC card supports HPI */
	if ((ext_csd[EXT_CSD_HPI_FEATURES] & 0x1) &&
		!(card->quirks & MMC_QUIRK_BROKEN_HPI)) {
		card->ext_csd.hpi = 1;
		if (ext_csd[EXT_CSD_HPI_FEATURES] & 0x2)
			card->ext_csd.hpi_cmd = MMC_STOP_TRANSMISSION;
		else
			card->ext_csd.hpi_cmd = MMC_SEND_STATUS;
		/*
		 * Indicate the maximum timeout to close
		 * a command interrupted by HPI
		 */
		card->ext_csd.out_of_int_time =
			ext_csd[EXT_CSD_OUT_OF_INTERRUPT_TIME] * 10;
		pr_info("%s: Out-of-interrupt timeout is %d[ms]\n",
				mmc_hostname(card->host),
				card->ext_csd.out_of_int_time);
	}

	if (card->ext_csd.rev >= 5) {
		/* Adjust production date as per JEDEC JESD84-B451 */
		if (card->cid.year < 2010)
			card->cid.year += 16;

		/* check whether the eMMC card supports BKOPS */
		if ((ext_csd[EXT_CSD_BKOPS_SUPPORT] & 0x1) &&
				card->ext_csd.hpi) {
			card->ext_csd.bkops = 1;
			card->ext_csd.bkops_en = ext_csd[EXT_CSD_BKOPS_EN];
			card->ext_csd.raw_bkops_status =
				ext_csd[EXT_CSD_BKOPS_STATUS];
			pr_info("%s: BKOPS_EN equals 0x%x\n",
					mmc_hostname(card->host),
					card->ext_csd.bkops_en);

		}

		card->ext_csd.rel_param = ext_csd[EXT_CSD_WR_REL_PARAM];
		card->ext_csd.rst_n_function = ext_csd[EXT_CSD_RST_N_FUNCTION];

		/*
		 * RPMB regions are defined in multiples of 128K.
		 */
		card->ext_csd.raw_rpmb_size_mult = ext_csd[EXT_CSD_RPMB_MULT];
		if (ext_csd[EXT_CSD_RPMB_MULT] && mmc_host_cmd23(card->host)) {
			mmc_part_add(card, ext_csd[EXT_CSD_RPMB_MULT] << 17,
				EXT_CSD_PART_CONFIG_ACC_RPMB,
				"rpmb", 0, false,
				MMC_BLK_DATA_AREA_RPMB);
		}
	}

	card->ext_csd.raw_erased_mem_count = ext_csd[EXT_CSD_ERASED_MEM_CONT];
	if (ext_csd[EXT_CSD_ERASED_MEM_CONT])
		card->erased_byte = 0xFF;
	else
		card->erased_byte = 0x0;

	/* eMMC v4.5 or later */
	if (card->ext_csd.rev >= 6) {
		card->ext_csd.feature_support |= MMC_DISCARD_FEATURE;

		card->ext_csd.generic_cmd6_time = 10 *
			ext_csd[EXT_CSD_GENERIC_CMD6_TIME];
		card->ext_csd.power_off_longtime = 10 *
			ext_csd[EXT_CSD_POWER_OFF_LONG_TIME];

		card->ext_csd.cache_size =
			ext_csd[EXT_CSD_CACHE_SIZE + 0] << 0 |
			ext_csd[EXT_CSD_CACHE_SIZE + 1] << 8 |
			ext_csd[EXT_CSD_CACHE_SIZE + 2] << 16 |
			ext_csd[EXT_CSD_CACHE_SIZE + 3] << 24;

		if (ext_csd[EXT_CSD_DATA_SECTOR_SIZE] == 1)
			card->ext_csd.data_sector_size = 4096;
		else
			card->ext_csd.data_sector_size = 512;

		if ((ext_csd[EXT_CSD_DATA_TAG_SUPPORT] & 1) &&
		    (ext_csd[EXT_CSD_TAG_UNIT_SIZE] <= 8)) {
			card->ext_csd.data_tag_unit_size =
			((unsigned int) 1 << ext_csd[EXT_CSD_TAG_UNIT_SIZE]) *
			(card->ext_csd.data_sector_size);
		} else {
			card->ext_csd.data_tag_unit_size = 0;
		}

		card->ext_csd.max_packed_writes =
			ext_csd[EXT_CSD_MAX_PACKED_WRITES];
		card->ext_csd.max_packed_reads =
			ext_csd[EXT_CSD_MAX_PACKED_READS];
	} else {
		card->ext_csd.data_sector_size = 512;
	}

	if (card->ext_csd.rev >= 7) {
		/* Enhance Strobe is supported since v5.1 which rev should be
		 * 8 but some eMMC devices can support it with rev 7. So handle
		 * Enhance Strobe here.
		 */
		card->ext_csd.strobe_support = ext_csd[EXT_CSD_STROBE_SUPPORT];
		card->ext_csd.cmdq_support = ext_csd[EXT_CSD_CMDQ_SUPPORT];
		if (card->ext_csd.cmdq_support) {
			/*
			 * Queue Depth = N + 1,
			 * see JEDEC JESD84-B51 section 7.4.19
			 */
			card->ext_csd.cmdq_depth =
				ext_csd[EXT_CSD_CMDQ_DEPTH] + 1;
			pr_info("%s: CMDQ supported: depth: %d\n",
				mmc_hostname(card->host),
				card->ext_csd.cmdq_depth);
		}
		card->ext_csd.barrier_support =
			ext_csd[EXT_CSD_BARRIER_SUPPORT];
		card->ext_csd.cache_flush_policy =
			ext_csd[EXT_CSD_CACHE_FLUSH_POLICY];
		pr_info("%s: cache barrier support %d flush policy %d\n",
				mmc_hostname(card->host),
				card->ext_csd.barrier_support,
				card->ext_csd.cache_flush_policy);
	} else {
		card->ext_csd.cmdq_support = 0;
		card->ext_csd.cmdq_depth = 0;
		card->ext_csd.barrier_support = 0;
		card->ext_csd.cache_flush_policy = 0;
	}

out:
	return err;
}

static inline void mmc_free_ext_csd(u8 *ext_csd)
{
	kfree(ext_csd);
}


static int mmc_compare_ext_csds(struct mmc_card *card, unsigned bus_width)
{
	u8 *bw_ext_csd;
	int err;

	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	err = mmc_get_ext_csd(card, &bw_ext_csd);

	if (err || bw_ext_csd == NULL) {
		err = -EINVAL;
		goto out;
	}

	/* only compare read only fields */
	err = !((card->ext_csd.raw_partition_support ==
			bw_ext_csd[EXT_CSD_PARTITION_SUPPORT]) &&
		(card->ext_csd.raw_erased_mem_count ==
			bw_ext_csd[EXT_CSD_ERASED_MEM_CONT]) &&
		(card->ext_csd.rev ==
			bw_ext_csd[EXT_CSD_REV]) &&
		(card->ext_csd.raw_ext_csd_structure ==
			bw_ext_csd[EXT_CSD_STRUCTURE]) &&
		(card->ext_csd.raw_card_type ==
			bw_ext_csd[EXT_CSD_CARD_TYPE]) &&
		(card->ext_csd.raw_s_a_timeout ==
			bw_ext_csd[EXT_CSD_S_A_TIMEOUT]) &&
		(card->ext_csd.raw_hc_erase_gap_size ==
			bw_ext_csd[EXT_CSD_HC_WP_GRP_SIZE]) &&
		(card->ext_csd.raw_erase_timeout_mult ==
			bw_ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT]) &&
		(card->ext_csd.raw_hc_erase_grp_size ==
			bw_ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]) &&
		(card->ext_csd.raw_sec_trim_mult ==
			bw_ext_csd[EXT_CSD_SEC_TRIM_MULT]) &&
		(card->ext_csd.raw_sec_erase_mult ==
			bw_ext_csd[EXT_CSD_SEC_ERASE_MULT]) &&
		(card->ext_csd.raw_sec_feature_support ==
			bw_ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT]) &&
		(card->ext_csd.raw_trim_mult ==
			bw_ext_csd[EXT_CSD_TRIM_MULT]) &&
		(card->ext_csd.raw_sectors[0] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 0]) &&
		(card->ext_csd.raw_sectors[1] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 1]) &&
		(card->ext_csd.raw_sectors[2] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 2]) &&
		(card->ext_csd.raw_sectors[3] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 3]) &&
		(card->ext_csd.raw_pwr_cl_52_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_52_195]) &&
		(card->ext_csd.raw_pwr_cl_26_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_26_195]) &&
		(card->ext_csd.raw_pwr_cl_52_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_52_360]) &&
		(card->ext_csd.raw_pwr_cl_26_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_26_360]) &&
		(card->ext_csd.raw_pwr_cl_200_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_200_195]) &&
		(card->ext_csd.raw_pwr_cl_200_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_200_360]) &&
		(card->ext_csd.raw_pwr_cl_ddr_52_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_52_195]) &&
		(card->ext_csd.raw_pwr_cl_ddr_52_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_52_360]) &&
		(card->ext_csd.raw_pwr_cl_ddr_200_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_200_360]));

	if (err)
		err = -EINVAL;

out:
	mmc_free_ext_csd(bw_ext_csd);
	return err;
}

MMC_DEV_ATTR(cid, "%08x%08x%08x%08x\n", card->raw_cid[0], card->raw_cid[1],
	card->raw_cid[2], card->raw_cid[3]);
MMC_DEV_ATTR(csd, "%08x%08x%08x%08x\n", card->raw_csd[0], card->raw_csd[1],
	card->raw_csd[2], card->raw_csd[3]);
MMC_DEV_ATTR(date, "%02d/%04d\n", card->cid.month, card->cid.year);
MMC_DEV_ATTR(erase_size, "%u\n", card->erase_size << 9);
MMC_DEV_ATTR(preferred_erase_size, "%u\n", card->pref_erase << 9);
MMC_DEV_ATTR(fwrev, "0x%x\n", card->cid.fwrev);
MMC_DEV_ATTR(hwrev, "0x%x\n", card->cid.hwrev);
MMC_DEV_ATTR(manfid, "0x%06x\n", card->cid.manfid);
MMC_DEV_ATTR(name, "%s\n", card->cid.prod_name);
MMC_DEV_ATTR(oemid, "0x%04x\n", card->cid.oemid);
MMC_DEV_ATTR(prv, "0x%x\n", card->cid.prv);
MMC_DEV_ATTR(serial, "0x%08x\n", card->cid.serial);
MMC_DEV_ATTR(enhanced_area_offset, "%llu\n",
		card->ext_csd.enhanced_area_offset);
MMC_DEV_ATTR(enhanced_area_size, "%u\n", card->ext_csd.enhanced_area_size);
MMC_DEV_ATTR(raw_rpmb_size_mult, "%#x\n", card->ext_csd.raw_rpmb_size_mult);
MMC_DEV_ATTR(rel_sectors, "%#x\n", card->ext_csd.rel_sectors);

static struct attribute *mmc_std_attrs[] = {
	&dev_attr_cid.attr,
	&dev_attr_csd.attr,
	&dev_attr_date.attr,
	&dev_attr_erase_size.attr,
	&dev_attr_preferred_erase_size.attr,
	&dev_attr_fwrev.attr,
	&dev_attr_hwrev.attr,
	&dev_attr_manfid.attr,
	&dev_attr_name.attr,
	&dev_attr_oemid.attr,
	&dev_attr_prv.attr,
	&dev_attr_serial.attr,
	&dev_attr_enhanced_area_offset.attr,
	&dev_attr_enhanced_area_size.attr,
	&dev_attr_raw_rpmb_size_mult.attr,
	&dev_attr_rel_sectors.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mmc_std);

static struct device_type mmc_type = {
	.groups = mmc_std_groups,
};

/*
 * Select the PowerClass for the current bus width
 * If power class is defined for 4/8 bit bus in the
 * extended CSD register, select it by executing the
 * mmc_switch command.
 */
static int __mmc_select_powerclass(struct mmc_card *card,
				   unsigned int bus_width)
{
	struct mmc_host *host = card->host;
	struct mmc_ext_csd *ext_csd = &card->ext_csd;
	unsigned int pwrclass_val = 0;
	int err = 0;

	/* Power class selection is supported for versions >= 4.0 */
	if (card->csd.mmca_vsn < CSD_SPEC_VER_4)
		return 0;

	/* Power class values are defined only for 4/8 bit bus */
	if (bus_width == EXT_CSD_BUS_WIDTH_1)
		return 0;

	switch (1 << host->ios.vdd) {
	case MMC_VDD_165_195:
		if (host->ios.clock <= MMC_HIGH_26_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_26_195;
		else if (host->ios.clock <= MMC_HIGH_52_MAX_DTR)
			pwrclass_val = (bus_width <= EXT_CSD_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_52_195 :
				ext_csd->raw_pwr_cl_ddr_52_195;
		else if (host->ios.clock <= MMC_HS200_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_200_195;
		break;
	case MMC_VDD_27_28:
	case MMC_VDD_28_29:
	case MMC_VDD_29_30:
	case MMC_VDD_30_31:
	case MMC_VDD_31_32:
	case MMC_VDD_32_33:
	case MMC_VDD_33_34:
	case MMC_VDD_34_35:
	case MMC_VDD_35_36:
		if (host->ios.clock <= MMC_HIGH_26_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_26_360;
		else if (host->ios.clock <= MMC_HIGH_52_MAX_DTR)
			pwrclass_val = (bus_width <= EXT_CSD_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_52_360 :
				ext_csd->raw_pwr_cl_ddr_52_360;
		else if (host->ios.clock <= MMC_HS200_MAX_DTR)
			pwrclass_val = (bus_width == EXT_CSD_DDR_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_ddr_200_360 :
				ext_csd->raw_pwr_cl_200_360;
		break;
	default:
		pr_warn("%s: Voltage range not supported for power class\n",
			mmc_hostname(host));
		return -EINVAL;
	}

	if (bus_width & (EXT_CSD_BUS_WIDTH_8 | EXT_CSD_DDR_BUS_WIDTH_8))
		pwrclass_val = (pwrclass_val & EXT_CSD_PWR_CL_8BIT_MASK) >>
				EXT_CSD_PWR_CL_8BIT_SHIFT;
	else
		pwrclass_val = (pwrclass_val & EXT_CSD_PWR_CL_4BIT_MASK) >>
				EXT_CSD_PWR_CL_4BIT_SHIFT;

	/* If the power class is different from the default value */
	if (pwrclass_val > 0) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_POWER_CLASS,
				 pwrclass_val,
				 card->ext_csd.generic_cmd6_time);
	}

	return err;
}

static int mmc_select_powerclass(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u32 bus_width, ext_csd_bits;
	int err, ddr;

	/* Power class selection is supported for versions >= 4.0 */
	if (!mmc_can_ext_csd(card))
		return 0;

	bus_width = host->ios.bus_width;
	/* Power class values are defined only for 4/8 bit bus */
	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	ddr = card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_52;
	if (ddr)
		ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
			EXT_CSD_DDR_BUS_WIDTH_8 : EXT_CSD_DDR_BUS_WIDTH_4;
	else
		ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
			EXT_CSD_BUS_WIDTH_8 :  EXT_CSD_BUS_WIDTH_4;

	err = __mmc_select_powerclass(card, ext_csd_bits);
	if (err)
		pr_warn("%s: power class selection to bus width %d ddr %d failed\n",
			mmc_hostname(host), 1 << bus_width, ddr);

	return err;
}

/*
 * Set the bus speed for the selected speed mode.
 */
static void mmc_set_bus_speed(struct mmc_card *card)
{
	unsigned int max_dtr = (unsigned int)-1;

	if ((mmc_card_hs200(card) || mmc_card_hs400(card)) &&
	     max_dtr > card->ext_csd.hs200_max_dtr)
		max_dtr = card->ext_csd.hs200_max_dtr;
	else if (mmc_card_hs(card) && max_dtr > card->ext_csd.hs_max_dtr)
		max_dtr = card->ext_csd.hs_max_dtr;
	else if (max_dtr > card->csd.max_dtr)
		max_dtr = card->csd.max_dtr;

	mmc_set_clock(card->host, max_dtr);
}

/*
 * Select the bus width amoung 4-bit and 8-bit(SDR).
 * If the bus width is changed successfully, return the selected width value.
 * Zero is returned instead of error value if the wide width is not supported.
 */
static int mmc_select_bus_width(struct mmc_card *card)
{
	static const unsigned ext_csd_bits[] = {
		EXT_CSD_BUS_WIDTH_8,
		EXT_CSD_BUS_WIDTH_4,
	};
	static const unsigned bus_widths[] = {
		MMC_BUS_WIDTH_8,
		MMC_BUS_WIDTH_4,
	};
	struct mmc_host *host = card->host;
	unsigned idx, bus_width = 0;
	int err = 0;

	if (!mmc_can_ext_csd(card) &&
	    !(host->caps & (MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA)))
		return 0;

	idx = (host->caps & MMC_CAP_8_BIT_DATA) ? 0 : 1;

	/*
	 * Unlike SD, MMC cards dont have a configuration register to notify
	 * supported bus width. So bus test command should be run to identify
	 * the supported bus width or compare the ext csd values of current
	 * bus width and ext csd values of 1 bit mode read earlier.
	 */
	for (; idx < ARRAY_SIZE(bus_widths); idx++) {
		/*
		 * Host is capable of 8bit transfer, then switch
		 * the device to work in 8bit transfer mode. If the
		 * mmc switch command returns error then switch to
		 * 4bit transfer mode. On success set the corresponding
		 * bus width on the host.
		 */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_BUS_WIDTH,
				 ext_csd_bits[idx],
				 card->ext_csd.generic_cmd6_time);
		if (err)
			continue;

		bus_width = bus_widths[idx];
		mmc_set_bus_width(host, bus_width);

		/*
		 * If controller can't handle bus width test,
		 * compare ext_csd previously read in 1 bit mode
		 * against ext_csd at new bus width
		 */
		if (!(host->caps & MMC_CAP_BUS_WIDTH_TEST))
			err = mmc_compare_ext_csds(card, bus_width);
		else
			err = mmc_bus_test(card, bus_width);

		if (!err) {
			err = bus_width;
			break;
		} else {
			pr_warn("%s: switch to bus width %d failed\n",
				mmc_hostname(host), ext_csd_bits[idx]);
		}
	}

	return err;
}

/*
 * Switch to the high-speed mode
 */
static int mmc_select_hs(struct mmc_card *card)
{
	int err;

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (!err)
		mmc_set_timing(card->host, MMC_TIMING_MMC_HS);

	return err;
}

/*
 * Activate wide bus and DDR if supported.
 */
static int mmc_select_hs_ddr(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u32 bus_width, ext_csd_bits;
	int err = 0;

	if (!(card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_52))
		return 0;

	bus_width = host->ios.bus_width;
	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
		EXT_CSD_DDR_BUS_WIDTH_8 : EXT_CSD_DDR_BUS_WIDTH_4;

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_BUS_WIDTH,
			ext_csd_bits,
			card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_warn("%s: switch to bus width %d ddr failed\n",
			mmc_hostname(host), 1 << bus_width);
		return err;
	}

	/*
	 * eMMC cards can support 3.3V to 1.2V i/o (vccq)
	 * signaling.
	 *
	 * EXT_CSD_CARD_TYPE_DDR_1_8V means 3.3V or 1.8V vccq.
	 *
	 * 1.8V vccq at 3.3V core voltage (vcc) is not required
	 * in the JEDEC spec for DDR.
	 *
	 * Even (e)MMC card can support 3.3v to 1.2v vccq, but not all
	 * host controller can support this, like some of the SDHCI
	 * controller which connect to an eMMC device. Some of these
	 * host controller still needs to use 1.8v vccq for supporting
	 * DDR mode.
	 *
	 * So the sequence will be:
	 * if (host and device can both support 1.2v IO)
	 *	use 1.2v IO;
	 * else if (host and device can both support 1.8v IO)
	 *	use 1.8v IO;
	 * so if host and device can only support 3.3v IO, this is the
	 * last choice.
	 *
	 * WARNING: eMMC rules are NOT the same as SD DDR
	 */
	err = -EINVAL;
	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_1_2V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_120);

	if (err && (card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_1_8V))
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180);

	/* make sure vccq is 3.3v after switching disaster */
	if (err)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_330);

	if (!err)
		mmc_set_timing(host, MMC_TIMING_MMC_DDR52);

	return err;
}

static int mmc_select_hs400(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;
	u8 val;

	/*
	 * HS400 mode requires 8-bit bus width
	 */
	if (card->ext_csd.strobe_support) {
		if (!(card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
		    host->caps & MMC_CAP_8_BIT_DATA))
			return 0;

		/* For Enhance Strobe flow. For non Enhance Strobe, signal
		 * voltage will not be set.
		 */
		if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_2V)
			err = __mmc_set_signal_voltage(host,
					MMC_SIGNAL_VOLTAGE_120);

		if (err && card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_8V)
			err = __mmc_set_signal_voltage(host,
					MMC_SIGNAL_VOLTAGE_180);
		if (err)
			return err;
	} else {
		if (!(card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
		    host->ios.bus_width == MMC_BUS_WIDTH_8))
			return 0;
	}

	/*
	 * Before switching to dual data rate operation for HS400,
	 * it is required to convert from HS200 mode to HS mode.
	 */
	mmc_set_timing(card->host, MMC_TIMING_MMC_HS);
	mmc_set_bus_speed(card);

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (err) {
		pr_warn("%s: switch to high-speed from hs200 failed, err:%d\n",
			mmc_hostname(host), err);
		return err;
	}

	val = EXT_CSD_DDR_BUS_WIDTH_8;
	if (card->ext_csd.strobe_support) {
		err = mmc_select_bus_width(card);
		if (IS_ERR_VALUE(err))
			return err;
		val |= EXT_CSD_BUS_WIDTH_STROBE;
	}
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_BUS_WIDTH,
			 val,
			 card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_warn("%s: switch to bus width for hs400 failed, err:%d\n",
			mmc_hostname(host), err);
		return err;
	}

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (err) {
		pr_warn("%s: switch to hs400 failed, err:%d\n",
			 mmc_hostname(host), err);
		return err;
	}

	mmc_set_timing(host, MMC_TIMING_MMC_HS400);
	mmc_set_bus_speed(card);

	if (host->ops->enhanced_strobe) {
		mmc_host_clk_hold(host);
		err = host->ops->enhanced_strobe(host);
		mmc_host_clk_release(host);
	} else if ((host->caps2 & MMC_CAP2_HS400_POST_TUNING) &&
			host->ops->execute_tuning) {
		mmc_host_clk_hold(host);
		err = host->ops->execute_tuning(host,
				MMC_SEND_TUNING_BLOCK_HS200);
		mmc_host_clk_release(host);

		if (err)
			pr_warn("%s: tuning execution failed\n",
				mmc_hostname(host));
	}

	return err;
}

/*
 * For device supporting HS200 mode, the following sequence
 * should be done before executing the tuning process.
 * 1. set the desired bus width(4-bit or 8-bit, 1-bit is not supported)
 * 2. switch to HS200 mode
 * 3. set the clock to > 52Mhz and <=200MHz
 */
static int mmc_select_hs200(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = -EINVAL;

	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_2V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_120);

	if (err && card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_8V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180);

	/* If fails try again during next card power cycle */
	if (err)
		goto err;

	/*
	 * Set the bus width(4 or 8) with host's support and
	 * switch to HS200 mode if bus width is set successfully.
	 */
	err = mmc_select_bus_width(card);
	if (!IS_ERR_VALUE(err)) {
		err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200,
				   card->ext_csd.generic_cmd6_time,
				   true, true, true);
		if (!err)
			mmc_set_timing(host, MMC_TIMING_MMC_HS200);
	}
err:
	return err;
}

static int mmc_reboot_notify(struct notifier_block *notify_block,
		unsigned long event, void *unused)
{
	struct mmc_card *card = container_of(
			notify_block, struct mmc_card, reboot_notify);

	if (event != SYS_RESTART)
		card->issue_long_pon = true;
	else
		card->issue_long_pon = false;

	return NOTIFY_OK;
}

/*
 * Activate High Speed or HS200 mode if supported.
 */
static int mmc_select_timing(struct mmc_card *card)
{
	int err = 0;

	if (!mmc_can_ext_csd(card))
		goto bus_speed;

	/* For Enhance Strobe HS400 flow */
	if (card->ext_csd.strobe_support &&
	    card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
	    card->host->caps & MMC_CAP_8_BIT_DATA)
		err = mmc_select_hs400(card);
	else if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200)
		err = mmc_select_hs200(card);
	else if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS)
		err = mmc_select_hs(card);

	if (err && err != -EBADMSG)
		return err;

	if (err) {
		pr_warn("%s: switch to %s failed\n",
			mmc_card_hs(card) ? "high-speed" :
			(mmc_card_hs200(card) ? "hs200" : ""),
			mmc_hostname(card->host));
		err = 0;
	}

bus_speed:
	/*
	 * Set the bus speed to the selected bus timing.
	 * If timing is not selected, backward compatible is the default.
	 */
	mmc_set_bus_speed(card);
	return err;
}

const u8 tuning_blk_pattern_4bit[MMC_TUNING_BLK_PATTERN_4BIT_SIZE] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};
EXPORT_SYMBOL(tuning_blk_pattern_4bit);

const u8 tuning_blk_pattern_8bit[MMC_TUNING_BLK_PATTERN_8BIT_SIZE] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};
EXPORT_SYMBOL(tuning_blk_pattern_8bit);

/*
 * Execute tuning sequence to seek the proper bus operating
 * conditions for HS200 and HS400, which sends CMD21 to the device.
 */
static int mmc_hs200_tuning(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	/*
	 * Timing should be adjusted to the HS400 target
	 * operation frequency for tuning process
	 */
	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
	    host->ios.bus_width == MMC_BUS_WIDTH_8)
		mmc_set_timing(host, MMC_TIMING_MMC_HS400);

	if (host->ops->execute_tuning) {
		mmc_host_clk_hold(host);
		err = host->ops->execute_tuning(host,
				MMC_SEND_TUNING_BLOCK_HS200);
		mmc_host_clk_release(host);

		if (err)
			pr_warn("%s: tuning execution failed\n",
				mmc_hostname(host));
	}

	return err;
}

static int mmc_select_cmdq(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int ret = 0;

	if (!host->cmdq_ops) {
		pr_err("%s: host controller doesn't support CMDQ\n",
		       mmc_hostname(host));
		return 0;
	}

	ret = mmc_set_blocklen(card, MMC_CARD_CMDQ_BLK_SIZE);
	if (ret)
		goto out;

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_CMDQ, 1,
			 card->ext_csd.generic_cmd6_time);
	if (ret)
		goto out;

	mmc_card_set_cmdq(card);
	mmc_host_clk_hold(card->host);
	ret = host->cmdq_ops->enable(card->host);
	if (ret) {
		mmc_host_clk_release(card->host);
		pr_err("%s: failed (%d) enabling CMDQ on host\n",
			mmc_hostname(host), ret);
		mmc_card_clr_cmdq(card);
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_CMDQ, 0,
				 card->ext_csd.generic_cmd6_time);
		if (ret)
			goto out;
	}

	mmc_host_clk_release(card->host);
	pr_info("%s: CMDQ enabled on card\n", mmc_hostname(host));
out:
	return ret;
}

/*
 * Scale down from HS400 to HS in order to allow frequency change.
 * This is needed for cards that doesn't support changing frequency in HS400
 */
static int mmc_scale_low(struct mmc_host *host, unsigned long freq)
{
	int err = 0;

	mmc_set_timing(host, MMC_TIMING_LEGACY);
	mmc_set_clock(host, MMC_HIGH_26_MAX_DTR);

	err = mmc_select_hs(host->card);
	if (err) {
		pr_err("%s: %s: selecting HS (52Mhz) failed (%d)\n",
			mmc_hostname(host), __func__, err);
		return err;
	}

	err = mmc_select_bus_width(host->card);
	if (err < 0) {
		pr_err("%s: %s: select_bus_width failed(%d)\n",
			mmc_hostname(host), __func__, err);
		return err;
	}

	mmc_set_clock(host, freq);

	return 0;
}

/*
 * Scale UP from HS to HS200/H400
 */
static int mmc_scale_high(struct mmc_host *host)
{
	int err = 0;

	if (!host->card->ext_csd.strobe_support) {
		if (!(host->card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200)) {
			pr_err("%s: %s: card does not support HS200\n",
				mmc_hostname(host), __func__);
			WARN_ON(1);
			return -EPERM;
		}

		err = mmc_select_hs200(host->card);
		if (err) {
			pr_err("%s: %s: selecting HS200 failed (%d)\n",
				mmc_hostname(host), __func__, err);
			return err;
		}

		mmc_set_bus_speed(host->card);

		err = mmc_hs200_tuning(host->card);
		if (err) {
			pr_err("%s: %s: hs200 tuning failed (%d)\n",
				mmc_hostname(host), __func__, err);
			return err;
		}

		if (!(host->card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400)) {
			pr_debug("%s: card does not support HS400\n",
				mmc_hostname(host));
			return 0;
		}
	}

	err = mmc_select_hs400(host->card);
	if (err) {
		pr_err("%s: %s: select hs400 failed (%d)\n",
			mmc_hostname(host), __func__, err);
		return err;
	}

	return 0;
}

static int mmc_set_clock_bus_speed(struct mmc_card *card, unsigned long freq)
{
	int err = 0;

	if (freq == MMC_HS200_MAX_DTR)
		err = mmc_scale_high(card->host);
	else
		err = mmc_scale_low(card->host, freq);

	return err;
}

static inline unsigned long mmc_ddr_freq_accommodation(unsigned long freq)
{
	if (freq == MMC_HIGH_DDR_MAX_DTR)
		return freq;

	return freq/2;
}

/**
 * mmc_change_bus_speed() - Change MMC card bus frequency at runtime
 * @host: pointer to mmc host structure
 * @freq: pointer to desired frequency to be set
 *
 * Change the MMC card bus frequency at runtime after the card is
 * initialized. Callers are expected to make sure of the card's
 * state (DATA/RCV/TRANSFER) before changing the frequency at runtime.
 *
 * If the frequency to change is greater than max. supported by card,
 * *freq is changed to max. supported by card. If it is less than min.
 * supported by host, *freq is changed to min. supported by host.
 * Host is assumed to be calimed while calling this funciton.
 */
static int mmc_change_bus_speed(struct mmc_host *host, unsigned long *freq)
{
	int err = 0;
	struct mmc_card *card;
	unsigned long actual_freq;

	card = host->card;

	if (!card || !freq) {
		err = -EINVAL;
		goto out;
	}
	actual_freq = *freq;

	WARN_ON(!host->claimed);

	/*
	 * For scaling up/down HS400 we'll need special handling,
	 * for other timings we can simply do clock frequency change
	 */
	if (mmc_card_hs400(card) ||
		(*freq == MMC_HS200_MAX_DTR)) {
		err = mmc_set_clock_bus_speed(card, *freq);
		if (err) {
			pr_err("%s: %s: failed (%d)to set bus and clock speed (freq=%lu)\n",
				mmc_hostname(host), __func__, err, *freq);
			goto out;
		}
	} else if (mmc_card_hs200(host->card)) {
		mmc_set_clock(host, *freq);
		err = mmc_hs200_tuning(host->card);
		if (err) {
			pr_warn("%s: %s: tuning execution failed %d\n",
				mmc_hostname(card->host),
				__func__, err);
			mmc_set_clock(host, host->clk_scaling.curr_freq);
		}
	} else {
		if (mmc_card_ddr52(host->card))
			actual_freq = mmc_ddr_freq_accommodation(*freq);
		mmc_set_clock(host, actual_freq);
	}

out:
	return err;
}

/*
 * Handle the detection and initialisation of a card.
 *
 * In the case of a resume, "oldcard" will contain the card
 * we're trying to reinitialise.
 */
static int mmc_init_card(struct mmc_host *host, u32 ocr,
	struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err;
	u32 cid[4];
	u32 rocr;
	u8 *ext_csd = NULL;

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	/* Set correct bus mode for MMC before attempting init */
	if (!mmc_host_is_spi(host))
		mmc_set_bus_mode(host, MMC_BUSMODE_OPENDRAIN);

	/*
	 * Since we're changing the OCR value, we seem to
	 * need to tell some cards to go back to the idle
	 * state.  We wait 1ms to give cards time to
	 * respond.
	 * mmc_go_idle is needed for eMMC that are asleep
	 */
reinit:
	mmc_go_idle(host);

	/* The extra bit indicates that we support high capacity */
	err = mmc_send_op_cond(host, ocr | (1 << 30), &rocr);
	if (err) {
		pr_err("%s: %s: mmc_send_op_cond() fails %d\n",
				mmc_hostname(host), __func__, err);
		goto err;
	}

	/*
	 * For SPI, enable CRC as appropriate.
	 */
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err) {
			pr_err("%s: %s: mmc_spi_set_crc() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto err;
		}
	}

	/*
	 * Fetch CID from card.
	 */
	if (mmc_host_is_spi(host))
		err = mmc_send_cid(host, cid);
	else
		err = mmc_all_send_cid(host, cid);
	if (err) {
		pr_err("%s: %s: mmc_send_cid() fails %d\n",
				mmc_hostname(host), __func__, err);
		goto err;
	}

	if (oldcard) {
		if (memcmp(cid, oldcard->raw_cid, sizeof(cid)) != 0) {
			err = -ENOENT;
			pr_err("%s: %s: CID memcmp failed %d\n",
					mmc_hostname(host), __func__, err);
			goto err;
		}

		card = oldcard;
	} else {
		/*
		 * Allocate card structure.
		 */
		card = mmc_alloc_card(host, &mmc_type);
		if (IS_ERR(card)) {
			err = PTR_ERR(card);
			pr_err("%s: %s: no memory to allocate for card %d\n",
					mmc_hostname(host), __func__, err);
			goto err;
		}

		card->ocr = ocr;
		card->type = MMC_TYPE_MMC;
		card->rca = 1;
		memcpy(card->raw_cid, cid, sizeof(card->raw_cid));
		host->card = card;
		card->reboot_notify.notifier_call = mmc_reboot_notify;
	}

	/*
	 * For native busses:  set card RCA and quit open drain mode.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_set_relative_addr(card);
		if (err) {
			pr_err("%s: %s: mmc_set_relative_addr() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}

		mmc_set_bus_mode(host, MMC_BUSMODE_PUSHPULL);
	}

	if (!oldcard) {
		/*
		 * Fetch CSD from card.
		 */
		err = mmc_send_csd(card, card->raw_csd);
		if (err) {
			pr_err("%s: %s: mmc_send_csd() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}

		err = mmc_decode_csd(card);
		if (err) {
			pr_err("%s: %s: mmc_decode_csd() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
		err = mmc_decode_cid(card);
		if (err) {
			pr_err("%s: %s: mmc_decode_cid() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
	}

	/*
	 * handling only for cards supporting DSR and hosts requesting
	 * DSR configuration
	 */
	if (card->csd.dsr_imp && host->dsr_req)
		mmc_set_dsr(host);

	/*
	 * Select card, as all following commands rely on that.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
		if (err) {
			pr_err("%s: %s: mmc_select_card() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
	}

	if (!oldcard) {
		/*
		 * Fetch and process extended CSD.
		 */

		err = mmc_get_ext_csd(card, &ext_csd);
		if (err) {
			pr_err("%s: %s: mmc_get_ext_csd() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
		card->cached_ext_csd = ext_csd;
		err = mmc_read_ext_csd(card, ext_csd);
		if (err) {
			pr_err("%s: %s: mmc_read_ext_csd() fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}

		/* If doing byte addressing, check if required to do sector
		 * addressing.  Handle the case of <2GB cards needing sector
		 * addressing.  See section 8.1 JEDEC Standard JED84-A441;
		 * ocr register has bit 30 set for sector addressing.
		 */
		if (!(mmc_card_blockaddr(card)) && (rocr & (1<<30)))
			mmc_card_set_blockaddr(card);

		/* Erase size depends on CSD and Extended CSD */
		mmc_set_erase_size(card);

		if (card->ext_csd.sectors && (rocr & MMC_CARD_SECTOR_ADDR))
			mmc_card_set_blockaddr(card);
	}

	/*
	 * If enhanced_area_en is TRUE, host needs to enable ERASE_GRP_DEF
	 * bit.  This bit will be lost every time after a reset or power off.
	 */
	if (card->ext_csd.partition_setting_completed ||
	    (card->ext_csd.rev >= 3 && (host->caps2 & MMC_CAP2_HC_ERASE_SZ))) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_ERASE_GROUP_DEF, 1,
				 card->ext_csd.generic_cmd6_time);

		if (err && err != -EBADMSG) {
			pr_err("%s: %s: mmc_switch() for ERASE_GRP_DEF fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}

		if (err) {
			err = 0;
			/*
			 * Just disable enhanced area off & sz
			 * will try to enable ERASE_GROUP_DEF
			 * during next time reinit
			 */
			card->ext_csd.enhanced_area_offset = -EINVAL;
			card->ext_csd.enhanced_area_size = -EINVAL;
		} else {
			card->ext_csd.erase_group_def = 1;
			/*
			 * enable ERASE_GRP_DEF successfully.
			 * This will affect the erase size, so
			 * here need to reset erase size
			 */
			mmc_set_erase_size(card);
		}
	}

	/*
	 * Ensure eMMC user default partition is enabled
	 */
	if (card->ext_csd.part_config & EXT_CSD_PART_CONFIG_ACC_MASK) {
		card->ext_csd.part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONFIG,
				 card->ext_csd.part_config,
				 card->ext_csd.part_time);
		if (err && err != -EBADMSG) {
			pr_err("%s: %s: mmc_switch() for PART_CONFIG fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
		card->part_curr = card->ext_csd.part_config &
				  EXT_CSD_PART_CONFIG_ACC_MASK;
	}

	/*
	 * Enable power_off_notification byte in the ext_csd register
	 */
	if (card->ext_csd.rev >= 6) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_POWER_OFF_NOTIFICATION,
				 EXT_CSD_POWER_ON,
				 card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG) {
			pr_err("%s: %s: mmc_switch() for POWER_ON PON fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}

		/*
		 * The err can be -EBADMSG or 0,
		 * so check for success and update the flag
		 */
		if (!err)
			card->ext_csd.power_off_notification = EXT_CSD_POWER_ON;
	}

	/*
	 * Select timing interface
	 */
	err = mmc_select_timing(card);
	if (err) {
		pr_err("%s: %s: mmc_select_timing() fails %d\n",
					mmc_hostname(host), __func__, err);
		goto free_card;
	}

	if (mmc_card_hs200(card)) {
		err = mmc_hs200_tuning(card);
		if (err)
			goto err;

		err = mmc_select_hs400(card);
		if (err)
			goto err;
	} else if (mmc_card_hs(card)) {
		/* Select the desired bus width optionally */
		err = mmc_select_bus_width(card);
		if (!IS_ERR_VALUE(err)) {
			err = mmc_select_hs_ddr(card);
			if (err)
				goto err;
		}
	}

	card->clk_scaling_lowest = host->f_min;
	if ((card->mmc_avail_type | EXT_CSD_CARD_TYPE_HS400) ||
			(card->mmc_avail_type | EXT_CSD_CARD_TYPE_HS200))
		card->clk_scaling_highest = card->ext_csd.hs200_max_dtr;
	else if ((card->mmc_avail_type | EXT_CSD_CARD_TYPE_HS) ||
			(card->mmc_avail_type | EXT_CSD_CARD_TYPE_DDR_52))
		card->clk_scaling_highest = card->ext_csd.hs_max_dtr;
	else
		card->clk_scaling_highest = card->csd.max_dtr;

	/*
	 * Choose the power class with selected bus interface
	 */
	mmc_select_powerclass(card);

	/*
	 * Enable HPI feature (if supported)
	 */
	if (card->ext_csd.hpi) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_HPI_MGMT, 1,
				card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG) {
			pr_err("%s: %s: mmc_switch() for HPI_MGMT fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
		if (err) {
			pr_warn("%s: Enabling HPI failed\n",
				mmc_hostname(card->host));
			err = 0;
		} else
			card->ext_csd.hpi_en = 1;
	}

	/*
	 * If cache size is higher than 0, this indicates
	 * the existence of cache and it can be turned on.
	 * If HPI is not supported then cache shouldn't be enabled.
	 */
	if (card->ext_csd.cache_size > 0) {
		if (card->ext_csd.hpi_en &&
			(!(card->quirks & MMC_QUIRK_CACHE_DISABLE))) {
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, 1,
					card->ext_csd.generic_cmd6_time);
			if (err && err != -EBADMSG) {
				pr_err("%s: %s: fail on CACHE_CTRL ON %d\n",
					mmc_hostname(host), __func__, err);
				goto free_card;
			}

			/*
			 * Only if no error, cache is turned on successfully.
			 */
			if (err) {
				pr_warn("%s: Cache is supported, but failed to turn on (%d)\n",
					mmc_hostname(card->host), err);
				card->ext_csd.cache_ctrl = 0;
				err = 0;
			} else {
				card->ext_csd.cache_ctrl = 1;
			}
			/* enable cache barrier if supported by the device */
			if (card->ext_csd.cache_ctrl &&
					card->ext_csd.barrier_support) {
				err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_BARRIER_CTRL, 1,
					card->ext_csd.generic_cmd6_time);
				if (err && err != -EBADMSG) {
					pr_err("%s: %s: mmc_switch() for BARRIER_CTRL fails %d\n",
						mmc_hostname(host), __func__,
						err);
					goto free_card;
				}
				if (err) {
					pr_warn("%s: Barrier is supported but failed to turn on (%d)\n",
						mmc_hostname(card->host), err);
					card->ext_csd.barrier_en = 0;
					err = 0;
				} else {
					card->ext_csd.barrier_en = 1;
				}
			}
		} else {
			/*
			 * mmc standard doesn't say what is the card default
			 * value for EXT_CSD_CACHE_CTRL.
			 * Hence, cache may be enabled by default by
			 * card vendors.
			 * Thus, it is best to explicitly disable cache in case
			 * we want to avoid cache.
			 */
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, 0,
					card->ext_csd.generic_cmd6_time);
			if (err) {
				pr_err("%s: %s: fail on CACHE_CTRL OFF %d\n",
					mmc_hostname(host), __func__, err);
				goto free_card;
			}
		}
	}

	/*
	 * The mandatory minimum values are defined for packed command.
	 * read: 5, write: 3
	 */
	if (card->ext_csd.max_packed_writes >= 3 &&
	    card->ext_csd.max_packed_reads >= 5 &&
	    host->caps2 & MMC_CAP2_PACKED_CMD) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_EXP_EVENTS_CTRL,
				EXT_CSD_PACKED_EVENT_EN,
				card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG) {
			pr_err("%s: %s: mmc_switch() for EXP_EVENTS_CTRL fails %d\n",
					mmc_hostname(host), __func__, err);
			goto free_card;
		}
		if (err) {
			pr_warn("%s: Enabling packed event failed\n",
				mmc_hostname(card->host));
			card->ext_csd.packed_event_en = 0;
			err = 0;
		} else {
			card->ext_csd.packed_event_en = 1;
		}

	}

	if (!oldcard) {
		if ((host->caps2 & MMC_CAP2_PACKED_CMD) &&
		    (card->ext_csd.max_packed_writes > 0)) {
			/*
			 * We would like to keep the statistics in an index
			 * that equals the num of packed requests
			 * (1 to max_packed_writes)
			 */
			card->wr_pack_stats.packing_events = kzalloc(
				(card->ext_csd.max_packed_writes + 1) *
				sizeof(*card->wr_pack_stats.packing_events),
				GFP_KERNEL);
			if (!card->wr_pack_stats.packing_events) {
				pr_err("%s: %s: no memory for packing events\n",
						mmc_hostname(host), __func__);
				goto free_card;
			}
		}
	}


	if (card->ext_csd.cmdq_support && (card->host->caps2 &
					   MMC_CAP2_CMD_QUEUE)) {
		err = mmc_select_cmdq(card);
		if (err) {
			pr_err("%s: selecting CMDQ mode: failed: %d\n",
					   mmc_hostname(card->host), err);
			card->ext_csd.cmdq_support = 0;
			oldcard = card;
			goto reinit;
		}
	}

	/*
	 * Start auto bkops, if supported.
	 *
	 * Note: This leaves the possibility of having both manual and
	 * auto bkops running in parallel. The runtime implementation
	 * will allow this, but ignore bkops exceptions on the premises
	 * that auto bkops will eventually kick in and the device will
	 * handle bkops without START_BKOPS from the host.
	 */
	if (mmc_card_support_auto_bkops(card)) {
		/*
		 * Ignore the return value of setting auto bkops.
		 * If it failed, will run in backward compatible mode.
		 */
		(void)mmc_set_auto_bkops(card, true);
	}

	return 0;

free_card:
	if (!oldcard) {
		host->card = NULL;
		mmc_remove_card(card);
	}
err:
	return err;
}

static int mmc_can_sleep(struct mmc_card *card)
{
	return (card && card->ext_csd.rev >= 3);
}

static int mmc_sleep(struct mmc_host *host)
{
	struct mmc_command cmd = {0};
	struct mmc_card *card = host->card;
	unsigned int timeout_ms = DIV_ROUND_UP(card->ext_csd.sa_timeout, 10000);
	int err;

	err = mmc_deselect_cards(host);
	if (err)
		return err;

	cmd.opcode = MMC_SLEEP_AWAKE;
	cmd.arg = card->rca << 16;
	cmd.arg |= 1 << 15;

	/*
	 * If the max_busy_timeout of the host is specified, validate it against
	 * the sleep cmd timeout. A failure means we need to prevent the host
	 * from doing hw busy detection, which is done by converting to a R1
	 * response instead of a R1B.
	 */
	if (host->max_busy_timeout && (timeout_ms > host->max_busy_timeout)) {
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
		cmd.busy_timeout = timeout_ms;
	}

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	/*
	 * If the host does not wait while the card signals busy, then we will
	 * will have to wait the sleep/awake timeout.  Note, we cannot use the
	 * SEND_STATUS command to poll the status because that command (and most
	 * others) is invalid while the card sleeps.
	 */
	if (!cmd.busy_timeout || !(host->caps & MMC_CAP_WAIT_WHILE_BUSY))
		mmc_delay(timeout_ms);

	return err;
}

static int mmc_can_poweroff_notify(const struct mmc_card *card)
{
	return card &&
		mmc_card_mmc(card) &&
		(card->ext_csd.power_off_notification == EXT_CSD_POWER_ON);
}

static int mmc_poweroff_notify(struct mmc_card *card, unsigned int notify_type)
{
	unsigned int timeout = card->ext_csd.generic_cmd6_time;
	int err;

	/* Use EXT_CSD_POWER_OFF_SHORT as default notification type. */
	if (notify_type == EXT_CSD_POWER_OFF_LONG)
		timeout = card->ext_csd.power_off_longtime;

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_POWER_OFF_NOTIFICATION,
			notify_type, timeout, true, false, false);
	if (err)
		pr_err("%s: Power Off Notification timed out, %u\n",
		       mmc_hostname(card->host), timeout);

	/* Disable the power off notification after the switch operation. */
	card->ext_csd.power_off_notification = EXT_CSD_NO_POWER_NOTIFICATION;

	return err;
}

int mmc_send_long_pon(struct mmc_card *card)
{
	int err = 0;
	struct mmc_host *host = card->host;

	mmc_claim_host(host);
	if (card->issue_long_pon && mmc_can_poweroff_notify(card)) {
		err = mmc_poweroff_notify(host->card, EXT_CSD_POWER_OFF_LONG);
		if (err)
			pr_warning("%s: error %d sending Long PON",
					mmc_hostname(host), err);
	}
	mmc_release_host(host);
	return err;
}

/*
 * Host is being removed. Free up the current card.
 */
static void mmc_remove(struct mmc_host *host)
{
	BUG_ON(!host);
	BUG_ON(!host->card);

	unregister_reboot_notifier(&host->card->reboot_notify);

	mmc_exit_clk_scaling(host);
	mmc_remove_card(host->card);

	mmc_claim_host(host);
	host->card = NULL;
	mmc_release_host(host);
}

/*
 * Card detection - card is alive.
 */
static int mmc_alive(struct mmc_host *host)
{
	return mmc_send_status(host->card, NULL);
}

/*
 * Card detection callback from host.
 */
static void mmc_detect(struct mmc_host *host)
{
	int err;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_get_card(host->card);

	/*
	 * Just check if our card has been removed.
	 */
	err = _mmc_detect_card_removed(host);

	mmc_put_card(host->card);

	if (err) {
		mmc_remove(host);

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

static int _mmc_suspend(struct mmc_host *host, bool is_suspend)
{
	int err = 0;

	BUG_ON(!host);
	BUG_ON(!host->card);

	/*
	 * Disable clock scaling before suspend and enable it after resume so
	 * as to avoid clock scaling decisions kicking in during this window.
	 */
	if (mmc_can_scale_clk(host))
		mmc_disable_clk_scaling(host);

	mmc_claim_host(host);

	if (mmc_card_suspended(host->card))
		goto out;

	if (mmc_card_cmdq(host->card)) {
		BUG_ON(host->cmdq_ctx.active_reqs);

		err = mmc_cmdq_halt(host, true);
		if (err) {
			pr_err("%s: halt: failed: %d\n", __func__, err);
			goto out;
		}
		mmc_host_clk_hold(host);
		host->cmdq_ops->disable(host, true);
		mmc_host_clk_release(host);
	}

	if (mmc_card_doing_bkops(host->card)) {
		err = mmc_stop_bkops(host->card);
		if (err)
			goto out;
	}

	if (mmc_card_doing_auto_bkops(host->card)) {
		err = mmc_set_auto_bkops(host->card, false);
		if (err)
			goto out;
	}

	err = mmc_flush_cache(host->card);
	if (err)
		goto out;

	if (mmc_can_sleep(host->card))
		err = mmc_sleep(host);
	else if (!mmc_host_is_spi(host))
		err = mmc_deselect_cards(host);

	if (!err) {
		mmc_power_off(host);
		mmc_card_set_suspended(host->card);
	}
out:
	mmc_release_host(host);
	return err;
}

/*
 * Suspend callback
 */
static int mmc_suspend(struct mmc_host *host)
{
	int err;
	ktime_t start = ktime_get();

	err = _mmc_suspend(host, true);
	if (!err) {
		pm_runtime_disable(&host->card->dev);
		pm_runtime_set_suspended(&host->card->dev);
	}

	trace_mmc_suspend(mmc_hostname(host), err,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return err;
}

/*
 * This function tries to determine if the same card is still present
 * and, if so, restore all state to it.
 */
static int _mmc_resume(struct mmc_host *host)
{
	int err = 0;
	int retries;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);

	if (!mmc_card_suspended(host->card)) {
		mmc_release_host(host);
		goto out;
	}

	mmc_power_up(host, host->card->ocr);
	retries = 3;
	while (retries) {
		err = mmc_init_card(host, host->card->ocr, host->card);
		if (err) {
			pr_err("%s: MMC card re-init failed rc = %d (retries = %d)\n",
			       mmc_hostname(host), err, retries);
			retries--;
			mmc_power_off(host);
			usleep_range(5000, 5500);
			mmc_power_up(host, host->card->ocr);
			mmc_select_voltage(host, host->card->ocr);
			continue;
		}
		break;
	}
	if (!err && mmc_card_cmdq(host->card)) {
		err = mmc_cmdq_halt(host, false);
		if (err)
			pr_err("%s: un-halt: failed: %d\n", __func__, err);
	}
	mmc_card_clr_suspended(host->card);

	mmc_release_host(host);

	/*
	 * We have done full initialization of the card,
	 * reset the clk scale stats and current frequency.
	 */
	if (mmc_can_scale_clk(host))
		mmc_init_clk_scaling(host);

out:
	return err;
}

/*
 * Shutdown callback
 */
static int mmc_shutdown(struct mmc_host *host)
{
	int err = 0;

	/*
	 * In a specific case for poweroff notify, we need to resume the card
	 * before we can shutdown it properly.
	 */
	if (mmc_can_poweroff_notify(host->card) &&
		!(host->caps2 & MMC_CAP2_FULL_PWR_CYCLE))
		err = _mmc_resume(host);

	if (!err)
		err = _mmc_suspend(host, false);

	return err;
}

/*
 * Callback for resume.
 */
static int mmc_resume(struct mmc_host *host)
{
	int err = 0;
	ktime_t start = ktime_get();

	if (!(host->caps & MMC_CAP_RUNTIME_RESUME)) {
		err = _mmc_resume(host);
		pm_runtime_set_active(&host->card->dev);
		pm_runtime_mark_last_busy(&host->card->dev);
	}
	pm_runtime_enable(&host->card->dev);

	trace_mmc_resume(mmc_hostname(host), err,
			ktime_to_us(ktime_sub(ktime_get(), start)));

	return err;
}

/*
 * Callback for runtime_suspend.
 */
static int mmc_runtime_suspend(struct mmc_host *host)
{
	int err;
	ktime_t start = ktime_get();

	if (!(host->caps & MMC_CAP_AGGRESSIVE_PM))
		return 0;

	err = _mmc_suspend(host, true);
	if (err)
		pr_err("%s: error %d doing aggessive suspend\n",
			mmc_hostname(host), err);

	trace_mmc_runtime_suspend(mmc_hostname(host), err,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return err;
}

/*
 * Callback for runtime_resume.
 */
static int mmc_runtime_resume(struct mmc_host *host)
{
	int err;
	ktime_t start = ktime_get();

	if (!(host->caps & (MMC_CAP_AGGRESSIVE_PM | MMC_CAP_RUNTIME_RESUME)))
		return 0;

	err = _mmc_resume(host);
	if (err)
		pr_err("%s: error %d doing aggessive resume\n",
			mmc_hostname(host), err);

	trace_mmc_runtime_resume(mmc_hostname(host), err,
			ktime_to_us(ktime_sub(ktime_get(), start)));

	return err;
}

static int mmc_power_restore(struct mmc_host *host)
{
	int ret;

	/* Disable clk scaling to avoid switching frequencies intermittently */
	mmc_disable_clk_scaling(host);

	mmc_claim_host(host);
	ret = mmc_init_card(host, host->card->ocr, host->card);
	mmc_release_host(host);

	if (mmc_can_scale_clk(host))
		mmc_init_clk_scaling(host);

	return ret;
}

#define NO_AUTOSUSPEND	(-1)
static int mmc_runtime_idle(struct mmc_host *host)
{
	BUG_ON(!host->card);

	if (host->card->bkops.needs_manual)
		mmc_start_manual_bkops(host->card);

	pm_runtime_mark_last_busy(&host->card->dev);
	/*
	 * TODO: consider prolonging suspend when bkops
	 * is running in order to allow a longer time for
	 * bkops to complete
	 * */
	pm_schedule_suspend(&host->card->dev, MMC_AUTOSUSPEND_DELAY_MS);

	/* return negative value in order to avoid autosuspend */
	return NO_AUTOSUSPEND;
}

static const struct mmc_bus_ops mmc_ops = {
	.remove = mmc_remove,
	.detect = mmc_detect,
	.suspend = mmc_suspend,
	.resume = mmc_resume,
	.runtime_suspend = mmc_runtime_suspend,
	.runtime_resume = mmc_runtime_resume,
	.runtime_idle = mmc_runtime_idle,
	.power_restore = mmc_power_restore,
	.alive = mmc_alive,
	.shutdown = mmc_shutdown,
	.change_bus_speed = mmc_change_bus_speed,
};

/*
 * Starting point for MMC card init.
 */
int mmc_attach_mmc(struct mmc_host *host)
{
	int err;
	u32 ocr, rocr;

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	/* Set correct bus mode for MMC before attempting attach */
	if (!mmc_host_is_spi(host))
		mmc_set_bus_mode(host, MMC_BUSMODE_OPENDRAIN);

	err = mmc_send_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus(host, &mmc_ops);
	if (host->ocr_avail_mmc)
		host->ocr_avail = host->ocr_avail_mmc;

	/*
	 * We need to get OCR a different way for SPI.
	 */
	if (mmc_host_is_spi(host)) {
		err = mmc_spi_read_ocr(host, 1, &ocr);
		if (err)
			goto err;
	}

	rocr = mmc_select_voltage(host, ocr);

	/*
	 * Can we support the voltage of the card?
	 */
	if (!rocr) {
		err = -EINVAL;
		goto err;
	}

	/*
	 * Detect and init the card.
	 */
	err = mmc_init_card(host, rocr, NULL);
	if (err)
		goto err;

	mmc_release_host(host);
	err = mmc_add_card(host->card);
	mmc_claim_host(host);
	if (err)
		goto remove_card;

	mmc_init_clk_scaling(host);

	register_reboot_notifier(&host->card->reboot_notify);

	return 0;

remove_card:
	mmc_release_host(host);
	mmc_remove_card(host->card);
	mmc_claim_host(host);
	host->card = NULL;
err:
	mmc_detach_bus(host);

	pr_err("%s: error %d whilst initialising MMC card\n",
		mmc_hostname(host), err);

	return err;
}
