/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 *
 * @File    ctatc.c
 *
 * @Brief
 * This file contains the implementation of the device resource management
 * object.
 *
 * @Author Liu Chun
 * @Date Mar 28 2008
 */

#include "ctatc.h"
#include "ctpcm.h"
#include "ctmixer.h"
#include "ctdrv.h"
#include "cthardware.h"
#include "ctsrc.h"
#include "ctamixer.h"
#include "ctdaio.h"
#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/asoundef.h>

#define MONO_SUM_SCALE	0x19a8	/* 2^(-0.5) in 14-bit floating format */
#define DAIONUM		7
#define MAX_MULTI_CHN	8

#define IEC958_DEFAULT_CON ((IEC958_AES0_NONAUDIO \
			    | IEC958_AES0_CON_NOT_COPYRIGHT) \
			    | ((IEC958_AES1_CON_MIXER \
			    | IEC958_AES1_CON_ORIGINAL) << 8) \
			    | (0x10 << 16) \
			    | ((IEC958_AES3_CON_FS_48000) << 24))

static const struct ct_atc_chip_sub_details atc_sub_details[NUM_CTCARDS] = {
	[CTSB0760] = {.subsys = PCI_SUBSYS_CREATIVE_SB0760,
		      .nm_model = "SB076x"},
	[CTHENDRIX] = {.subsys = PCI_SUBSYS_CREATIVE_HENDRIX,
		      .nm_model = "Hendrix"},
	[CTSB08801] = {.subsys = PCI_SUBSYS_CREATIVE_SB08801,
		      .nm_model = "SB0880"},
	[CTSB08802] = {.subsys = PCI_SUBSYS_CREATIVE_SB08802,
		      .nm_model = "SB0880"},
	[CTSB08803] = {.subsys = PCI_SUBSYS_CREATIVE_SB08803,
		      .nm_model = "SB0880"}
};

static struct ct_atc_chip_details atc_chip_details[] = {
	{.vendor = PCI_VENDOR_CREATIVE, .device = PCI_DEVICE_CREATIVE_20K1,
	 .sub_details = NULL,
	 .nm_card = "X-Fi 20k1"},
	{.vendor = PCI_VENDOR_CREATIVE, .device = PCI_DEVICE_CREATIVE_20K2,
	 .sub_details = atc_sub_details,
	 .nm_card = "X-Fi 20k2"},
	{} /* terminator */
};

static struct {
	int (*create)(struct ct_atc *atc,
			enum CTALSADEVS device, const char *device_name);
	int (*destroy)(void *alsa_dev);
	const char *public_name;
} alsa_dev_funcs[NUM_CTALSADEVS] = {
	[FRONT]		= { .create = ct_alsa_pcm_create,
			    .destroy = NULL,
			    .public_name = "Front/WaveIn"},
	[REAR]		= { .create = ct_alsa_pcm_create,
			    .destroy = NULL,
			    .public_name = "Rear"},
	[CLFE]		= { .create = ct_alsa_pcm_create,
			    .destroy = NULL,
			    .public_name = "Center/LFE"},
	[SURROUND]	= { .create = ct_alsa_pcm_create,
			    .destroy = NULL,
			    .public_name = "Surround"},
	[IEC958]	= { .create = ct_alsa_pcm_create,
			    .destroy = NULL,
			    .public_name = "IEC958 Non-audio"},

	[MIXER]		= { .create = ct_alsa_mix_create,
			    .destroy = NULL,
			    .public_name = "Mixer"}
};

typedef int (*create_t)(void *, void **);
typedef int (*destroy_t)(void *);

static struct {
	int (*create)(void *hw, void **rmgr);
	int (*destroy)(void *mgr);
} rsc_mgr_funcs[NUM_RSCTYP] = {
	[SRC] 		= { .create 	= (create_t)src_mgr_create,
			    .destroy 	= (destroy_t)src_mgr_destroy	},
	[SRCIMP] 	= { .create 	= (create_t)srcimp_mgr_create,
			    .destroy 	= (destroy_t)srcimp_mgr_destroy	},
	[AMIXER]	= { .create	= (create_t)amixer_mgr_create,
			    .destroy	= (destroy_t)amixer_mgr_destroy	},
	[SUM]		= { .create	= (create_t)sum_mgr_create,
			    .destroy	= (destroy_t)sum_mgr_destroy	},
	[DAIO]		= { .create	= (create_t)daio_mgr_create,
			    .destroy	= (destroy_t)daio_mgr_destroy	}
};

static int
atc_pcm_release_resources(struct ct_atc *atc, struct ct_atc_pcm *apcm);

/* *
 * Only mono and interleaved modes are supported now.
 * Always allocates a contiguous channel block.
 * */

static int ct_map_audio_buffer(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	unsigned long flags;
	struct snd_pcm_runtime *runtime;
	struct ct_vm *vm;

	if (NULL == apcm->substream)
		return 0;

	runtime = apcm->substream->runtime;
	vm = atc->vm;

	spin_lock_irqsave(&atc->vm_lock, flags);
	apcm->vm_block = vm->map(vm, runtime->dma_area, runtime->dma_bytes);
	spin_unlock_irqrestore(&atc->vm_lock, flags);

	if (NULL == apcm->vm_block)
		return -ENOENT;

	return 0;
}

static void ct_unmap_audio_buffer(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	unsigned long flags;
	struct ct_vm *vm;

	if (NULL == apcm->vm_block)
		return;

	vm = atc->vm;

	spin_lock_irqsave(&atc->vm_lock, flags);
	vm->unmap(vm, apcm->vm_block);
	spin_unlock_irqrestore(&atc->vm_lock, flags);

	apcm->vm_block = NULL;
}

static unsigned long atc_get_ptp_phys(struct ct_atc *atc, int index)
{
	struct ct_vm *vm;
	void *kvirt_addr;
	unsigned long phys_addr;
	unsigned long flags;

	spin_lock_irqsave(&atc->vm_lock, flags);
	vm = atc->vm;
	kvirt_addr = vm->get_ptp_virt(vm, index);
	if (kvirt_addr == NULL)
		phys_addr = (~0UL);
	else
		phys_addr = virt_to_phys(kvirt_addr);

	spin_unlock_irqrestore(&atc->vm_lock, flags);

	return phys_addr;
}

static unsigned int convert_format(snd_pcm_format_t snd_format)
{
	switch (snd_format) {
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S8:
		return SRC_SF_U8;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_U16_LE:
		return SRC_SF_S16;
	case SNDRV_PCM_FORMAT_S24_3LE:
		return SRC_SF_S24;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		return SRC_SF_S32;
	default:
		printk(KERN_ERR "not recognized snd format is %d \n",
			snd_format);
		return SRC_SF_S16;
	}
}

static unsigned int
atc_get_pitch(unsigned int input_rate, unsigned int output_rate)
{
	unsigned int pitch = 0;
	int b = 0;

	/* get pitch and convert to fixed-point 8.24 format. */
	pitch = (input_rate / output_rate) << 24;
	input_rate %= output_rate;
	input_rate /= 100;
	output_rate /= 100;
	for (b = 31; ((b >= 0) && !(input_rate >> b)); )
		b--;

	if (b >= 0) {
		input_rate <<= (31 - b);
		input_rate /= output_rate;
		b = 24 - (31 - b);
		if (b >= 0)
			input_rate <<= b;
		else
			input_rate >>= -b;

		pitch |= input_rate;
	}

	return pitch;
}

static int select_rom(unsigned int pitch)
{
	if ((pitch > 0x00428f5c) && (pitch < 0x01b851ec)) {
		/* 0.26 <= pitch <= 1.72 */
		return 1;
	} else if ((0x01d66666 == pitch) || (0x01d66667 == pitch)) {
		/* pitch == 1.8375 */
		return 2;
	} else if (0x02000000 == pitch) {
		/* pitch == 2 */
		return 3;
	} else if ((pitch >= 0x0) && (pitch <= 0x08000000)) {
		/* 0 <= pitch <= 8 */
		return 0;
	} else {
		return -ENOENT;
	}
}

static int atc_pcm_playback_prepare(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src_mgr *src_mgr = atc->rsc_mgrs[SRC];
	struct amixer_mgr *amixer_mgr = atc->rsc_mgrs[AMIXER];
	struct src_desc desc = {0};
	struct amixer_desc mix_dsc = {0};
	struct src *src = NULL;
	struct amixer *amixer = NULL;
	int err = 0;
	int n_amixer = apcm->substream->runtime->channels, i = 0;
	int device = apcm->substream->pcm->device;
	unsigned int pitch = 0;
	unsigned long flags;

	if (NULL != apcm->src) {
		/* Prepared pcm playback */
		return 0;
	}

	/* Get SRC resource */
	desc.multi = apcm->substream->runtime->channels;
	desc.msr = atc->msr;
	desc.mode = MEMRD;
	err = src_mgr->get_src(src_mgr, &desc, (struct src **)&apcm->src);
	if (err)
		goto error1;

	pitch = atc_get_pitch(apcm->substream->runtime->rate,
						(atc->rsr * atc->msr));
	src = apcm->src;
	src->ops->set_pitch(src, pitch);
	src->ops->set_rom(src, select_rom(pitch));
	src->ops->set_sf(src, convert_format(apcm->substream->runtime->format));
	src->ops->set_pm(src, (src->ops->next_interleave(src) != NULL));

	/* Get AMIXER resource */
	n_amixer = (n_amixer < 2) ? 2 : n_amixer;
	apcm->amixers = kzalloc(sizeof(void *)*n_amixer, GFP_KERNEL);
	if (NULL == apcm->amixers) {
		err = -ENOMEM;
		goto error1;
	}
	mix_dsc.msr = atc->msr;
	for (i = 0, apcm->n_amixer = 0; i < n_amixer; i++) {
		err = amixer_mgr->get_amixer(amixer_mgr, &mix_dsc,
					(struct amixer **)&apcm->amixers[i]);
		if (err)
			goto error1;

		apcm->n_amixer++;
	}

	/* Set up device virtual mem map */
	err = ct_map_audio_buffer(atc, apcm);
	if (err < 0)
		goto error1;

	/* Connect resources */
	src = apcm->src;
	for (i = 0; i < n_amixer; i++) {
		amixer = apcm->amixers[i];
		spin_lock_irqsave(&atc->atc_lock, flags);
		amixer->ops->setup(amixer, &src->rsc,
					INIT_VOL, atc->pcm[i+device*2]);
		spin_unlock_irqrestore(&atc->atc_lock, flags);
		src = src->ops->next_interleave(src);
		if (NULL == src)
			src = apcm->src;
	}

	return 0;

error1:
	atc_pcm_release_resources(atc, apcm);
	return err;
}

static int
atc_pcm_release_resources(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src_mgr *src_mgr = atc->rsc_mgrs[SRC];
	struct srcimp_mgr *srcimp_mgr = atc->rsc_mgrs[SRCIMP];
	struct amixer_mgr *amixer_mgr = atc->rsc_mgrs[AMIXER];
	struct sum_mgr *sum_mgr = atc->rsc_mgrs[SUM];
	struct srcimp *srcimp = NULL;
	int i = 0;

	if (NULL != apcm->srcimps) {
		for (i = 0; i < apcm->n_srcimp; i++) {
			srcimp = apcm->srcimps[i];
			srcimp->ops->unmap(srcimp);
			srcimp_mgr->put_srcimp(srcimp_mgr, srcimp);
			apcm->srcimps[i] = NULL;
		}
		kfree(apcm->srcimps);
		apcm->srcimps = NULL;
	}

	if (NULL != apcm->srccs) {
		for (i = 0; i < apcm->n_srcc; i++) {
			src_mgr->put_src(src_mgr, apcm->srccs[i]);
			apcm->srccs[i] = NULL;
		}
		kfree(apcm->srccs);
		apcm->srccs = NULL;
	}

	if (NULL != apcm->amixers) {
		for (i = 0; i < apcm->n_amixer; i++) {
			amixer_mgr->put_amixer(amixer_mgr, apcm->amixers[i]);
			apcm->amixers[i] = NULL;
		}
		kfree(apcm->amixers);
		apcm->amixers = NULL;
	}

	if (NULL != apcm->mono) {
		sum_mgr->put_sum(sum_mgr, apcm->mono);
		apcm->mono = NULL;
	}

	if (NULL != apcm->src) {
		src_mgr->put_src(src_mgr, apcm->src);
		apcm->src = NULL;
	}

	if (NULL != apcm->vm_block) {
		/* Undo device virtual mem map */
		ct_unmap_audio_buffer(atc, apcm);
		apcm->vm_block = NULL;
	}

	return 0;
}

static int atc_pcm_playback_start(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	unsigned int max_cisz = 0;
	struct src *src = apcm->src;

	max_cisz = src->multi * src->rsc.msr;
	max_cisz = 0x80 * (max_cisz < 8 ? max_cisz : 8);

	src->ops->set_sa(src, apcm->vm_block->addr);
	src->ops->set_la(src, apcm->vm_block->addr + apcm->vm_block->size);
	src->ops->set_ca(src, apcm->vm_block->addr + max_cisz);
	src->ops->set_cisz(src, max_cisz);

	src->ops->set_bm(src, 1);
	src->ops->set_state(src, SRC_STATE_INIT);
	src->ops->commit_write(src);

	return 0;
}

static int atc_pcm_stop(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = NULL;
	int i = 0;

	src = apcm->src;
	src->ops->set_bm(src, 0);
	src->ops->set_state(src, SRC_STATE_OFF);
	src->ops->commit_write(src);

	if (NULL != apcm->srccs) {
		for (i = 0; i < apcm->n_srcc; i++) {
			src = apcm->srccs[i];
			src->ops->set_bm(src, 0);
			src->ops->set_state(src, SRC_STATE_OFF);
			src->ops->commit_write(src);
		}
	}

	apcm->started = 0;

	return 0;
}

static int
atc_pcm_playback_position(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = apcm->src;
	u32 size = 0, max_cisz = 0;
	int position = 0;

	position = src->ops->get_ca(src);

	size = apcm->vm_block->size;
	max_cisz = src->multi * src->rsc.msr;
	max_cisz = 128 * (max_cisz < 8 ? max_cisz : 8);

	return (position + size - max_cisz - apcm->vm_block->addr) % size;
}

struct src_node_conf_t {
	unsigned int pitch;
	unsigned int msr:8;
	unsigned int mix_msr:8;
	unsigned int imp_msr:8;
	unsigned int vo:1;
};

static void setup_src_node_conf(struct ct_atc *atc, struct ct_atc_pcm *apcm,
				struct src_node_conf_t *conf, int *n_srcc)
{
	unsigned int pitch = 0;

	/* get pitch and convert to fixed-point 8.24 format. */
	pitch = atc_get_pitch((atc->rsr * atc->msr),
				apcm->substream->runtime->rate);
	*n_srcc = 0;

	if (1 == atc->msr) {
		*n_srcc = apcm->substream->runtime->channels;
		conf[0].pitch = pitch;
		conf[0].mix_msr = conf[0].imp_msr = conf[0].msr = 1;
		conf[0].vo = 1;
	} else if (2 == atc->msr) {
		if (0x8000000 < pitch) {
			/* Need two-stage SRCs, SRCIMPs and
			 * AMIXERs for converting format */
			conf[0].pitch = (atc->msr << 24);
			conf[0].msr = conf[0].mix_msr = 1;
			conf[0].imp_msr = atc->msr;
			conf[0].vo = 0;
			conf[1].pitch = atc_get_pitch(atc->rsr,
					apcm->substream->runtime->rate);
			conf[1].msr = conf[1].mix_msr = conf[1].imp_msr = 1;
			conf[1].vo = 1;
			*n_srcc = apcm->substream->runtime->channels * 2;
		} else if (0x1000000 < pitch) {
			/* Need one-stage SRCs, SRCIMPs and
			 * AMIXERs for converting format */
			conf[0].pitch = pitch;
			conf[0].msr = conf[0].mix_msr
				    = conf[0].imp_msr = atc->msr;
			conf[0].vo = 1;
			*n_srcc = apcm->substream->runtime->channels;
		}
	}
}

static int
atc_pcm_capture_get_resources(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src_mgr *src_mgr = atc->rsc_mgrs[SRC];
	struct srcimp_mgr *srcimp_mgr = atc->rsc_mgrs[SRCIMP];
	struct amixer_mgr *amixer_mgr = atc->rsc_mgrs[AMIXER];
	struct sum_mgr *sum_mgr = atc->rsc_mgrs[SUM];
	struct src_desc src_dsc = {0};
	struct src *src = NULL;
	struct srcimp_desc srcimp_dsc = {0};
	struct srcimp *srcimp = NULL;
	struct amixer_desc mix_dsc = {0};
	struct sum_desc sum_dsc = {0};
	unsigned int pitch = 0;
	int multi = 0, err = 0, i = 0;
	int n_srcimp = 0, n_amixer = 0, n_srcc = 0, n_sum = 0;
	struct src_node_conf_t src_node_conf[2] = {{0} };

	/* The numbers of converting SRCs and SRCIMPs should be determined
	 * by pitch value. */

	multi = apcm->substream->runtime->channels;

	/* get pitch and convert to fixed-point 8.24 format. */
	pitch = atc_get_pitch((atc->rsr * atc->msr),
				apcm->substream->runtime->rate);

	setup_src_node_conf(atc, apcm, src_node_conf, &n_srcc);
	n_sum = (1 == multi) ? 1 : 0;
	n_amixer += n_sum * 2 + n_srcc;
	n_srcimp += n_srcc;
	if ((multi > 1) && (0x8000000 >= pitch)) {
		/* Need extra AMIXERs and SRCIMPs for special treatment
		 * of interleaved recording of conjugate channels */
		n_amixer += multi * atc->msr;
		n_srcimp += multi * atc->msr;
	} else {
		n_srcimp += multi;
	}

	if (n_srcc) {
		apcm->srccs = kzalloc(sizeof(void *)*n_srcc, GFP_KERNEL);
		if (NULL == apcm->srccs)
			return -ENOMEM;
	}
	if (n_amixer) {
		apcm->amixers = kzalloc(sizeof(void *)*n_amixer, GFP_KERNEL);
		if (NULL == apcm->amixers) {
			err = -ENOMEM;
			goto error1;
		}
	}
	apcm->srcimps = kzalloc(sizeof(void *)*n_srcimp, GFP_KERNEL);
	if (NULL == apcm->srcimps) {
		err = -ENOMEM;
		goto error1;
	}

	/* Allocate SRCs for sample rate conversion if needed */
	src_dsc.multi = 1;
	src_dsc.mode = ARCRW;
	for (i = 0, apcm->n_srcc = 0; i < n_srcc; i++) {
		src_dsc.msr = src_node_conf[i/multi].msr;
		err = src_mgr->get_src(src_mgr, &src_dsc,
					(struct src **)&apcm->srccs[i]);
		if (err)
			goto error1;

		src = apcm->srccs[i];
		pitch = src_node_conf[i/multi].pitch;
		src->ops->set_pitch(src, pitch);
		src->ops->set_rom(src, select_rom(pitch));
		src->ops->set_vo(src, src_node_conf[i/multi].vo);

		apcm->n_srcc++;
	}

	/* Allocate AMIXERs for routing SRCs of conversion if needed */
	for (i = 0, apcm->n_amixer = 0; i < n_amixer; i++) {
		if (i < (n_sum*2))
			mix_dsc.msr = atc->msr;
		else if (i < (n_sum*2+n_srcc))
			mix_dsc.msr = src_node_conf[(i-n_sum*2)/multi].mix_msr;
		else
			mix_dsc.msr = 1;

		err = amixer_mgr->get_amixer(amixer_mgr, &mix_dsc,
					(struct amixer **)&apcm->amixers[i]);
		if (err)
			goto error1;

		apcm->n_amixer++;
	}

	/* Allocate a SUM resource to mix all input channels together */
	sum_dsc.msr = atc->msr;
	err = sum_mgr->get_sum(sum_mgr, &sum_dsc, (struct sum **)&apcm->mono);
	if (err)
		goto error1;

	pitch = atc_get_pitch((atc->rsr * atc->msr),
				apcm->substream->runtime->rate);
	/* Allocate SRCIMP resources */
	for (i = 0, apcm->n_srcimp = 0; i < n_srcimp; i++) {
		if (i < (n_srcc))
			srcimp_dsc.msr = src_node_conf[i/multi].imp_msr;
		else if (1 == multi)
			srcimp_dsc.msr = (pitch <= 0x8000000) ? atc->msr : 1;
		else
			srcimp_dsc.msr = 1;

		err = srcimp_mgr->get_srcimp(srcimp_mgr, &srcimp_dsc, &srcimp);
		if (err)
			goto error1;

		apcm->srcimps[i] = srcimp;
		apcm->n_srcimp++;
	}

	/* Allocate a SRC for writing data to host memory */
	src_dsc.multi = apcm->substream->runtime->channels;
	src_dsc.msr = 1;
	src_dsc.mode = MEMWR;
	err = src_mgr->get_src(src_mgr, &src_dsc, (struct src **)&apcm->src);
	if (err)
		goto error1;

	src = apcm->src;
	src->ops->set_pitch(src, pitch);

	/* Set up device virtual mem map */
	err = ct_map_audio_buffer(atc, apcm);
	if (err < 0)
		goto error1;

	return 0;

error1:
	atc_pcm_release_resources(atc, apcm);
	return err;
}

static int atc_pcm_capture_prepare(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = NULL;
	struct amixer *amixer = NULL;
	struct srcimp *srcimp = NULL;
	struct ct_mixer *mixer = atc->mixer;
	struct sum *mono = NULL;
	struct rsc *out_ports[8] = {NULL};
	int err = 0, i = 0, j = 0, n_sum = 0, multi = 0;
	unsigned int pitch = 0;
	int mix_base = 0, imp_base = 0;

	if (NULL != apcm->src) {
		/* Prepared pcm capture */
		return 0;
	}

	/* Get needed resources. */
	err = atc_pcm_capture_get_resources(atc, apcm);
	if (err)
		return err;

	/* Connect resources */
	mixer->get_output_ports(mixer, MIX_PCMO_FRONT,
				&out_ports[0], &out_ports[1]);

	multi = apcm->substream->runtime->channels;
	if (1 == multi) {
		mono = apcm->mono;
		for (i = 0; i < 2; i++) {
			amixer = apcm->amixers[i];
			amixer->ops->setup(amixer, out_ports[i],
						MONO_SUM_SCALE, mono);
		}
		out_ports[0] = &mono->rsc;
		n_sum = 1;
		mix_base = n_sum * 2;
	}

	for (i = 0; i < apcm->n_srcc; i++) {
		src = apcm->srccs[i];
		srcimp = apcm->srcimps[imp_base+i];
		amixer = apcm->amixers[mix_base+i];
		srcimp->ops->map(srcimp, src, out_ports[i%multi]);
		amixer->ops->setup(amixer, &src->rsc, INIT_VOL, NULL);
		out_ports[i%multi] = &amixer->rsc;
	}

	pitch = atc_get_pitch((atc->rsr * atc->msr),
				apcm->substream->runtime->rate);

	if ((multi > 1) && (pitch <= 0x8000000)) {
		/* Special connection for interleaved
		 * recording with conjugate channels */
		for (i = 0; i < multi; i++) {
			out_ports[i]->ops->master(out_ports[i]);
			for (j = 0; j < atc->msr; j++) {
				amixer = apcm->amixers[apcm->n_srcc+j*multi+i];
				amixer->ops->set_input(amixer, out_ports[i]);
				amixer->ops->set_scale(amixer, INIT_VOL);
				amixer->ops->set_sum(amixer, NULL);
				amixer->ops->commit_raw_write(amixer);
				out_ports[i]->ops->next_conj(out_ports[i]);

				srcimp = apcm->srcimps[apcm->n_srcc+j*multi+i];
				srcimp->ops->map(srcimp, apcm->src,
							&amixer->rsc);
			}
		}
	} else {
		for (i = 0; i < multi; i++) {
			srcimp = apcm->srcimps[apcm->n_srcc+i];
			srcimp->ops->map(srcimp, apcm->src, out_ports[i]);
		}
	}

	return 0;
}

static int atc_pcm_capture_start(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = NULL;
	struct src_mgr *src_mgr = atc->rsc_mgrs[SRC];
	int i = 0, multi = 0;

	if (apcm->started)
		return 0;

	apcm->started = 1;
	multi = apcm->substream->runtime->channels;
	/* Set up converting SRCs */
	for (i = 0; i < apcm->n_srcc; i++) {
		src = apcm->srccs[i];
		src->ops->set_pm(src, ((i%multi) != (multi-1)));
		src_mgr->src_disable(src_mgr, src);
	}

	/*  Set up recording SRC */
	src = apcm->src;
	src->ops->set_sf(src, convert_format(apcm->substream->runtime->format));
	src->ops->set_sa(src, apcm->vm_block->addr);
	src->ops->set_la(src, apcm->vm_block->addr + apcm->vm_block->size);
	src->ops->set_ca(src, apcm->vm_block->addr);
	src_mgr->src_disable(src_mgr, src);

	/* Disable relevant SRCs firstly */
	src_mgr->commit_write(src_mgr);

	/* Enable SRCs respectively */
	for (i = 0; i < apcm->n_srcc; i++) {
		src = apcm->srccs[i];
		src->ops->set_state(src, SRC_STATE_RUN);
		src->ops->commit_write(src);
		src_mgr->src_enable_s(src_mgr, src);
	}
	src = apcm->src;
	src->ops->set_bm(src, 1);
	src->ops->set_state(src, SRC_STATE_RUN);
	src->ops->commit_write(src);
	src_mgr->src_enable_s(src_mgr, src);

	/* Enable relevant SRCs synchronously */
	src_mgr->commit_write(src_mgr);

	return 0;
}

static int
atc_pcm_capture_position(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = apcm->src;

	return src->ops->get_ca(src) - apcm->vm_block->addr;
}

static int spdif_passthru_playback_get_resources(struct ct_atc *atc,
						 struct ct_atc_pcm *apcm)
{
	struct src_mgr *src_mgr = atc->rsc_mgrs[SRC];
	struct amixer_mgr *amixer_mgr = atc->rsc_mgrs[AMIXER];
	struct src_desc desc = {0};
	struct amixer_desc mix_dsc = {0};
	struct src *src = NULL;
	int err = 0;
	int n_amixer = apcm->substream->runtime->channels, i = 0;
	unsigned int pitch = 0, rsr = atc->pll_rate;

	/* Get SRC resource */
	desc.multi = apcm->substream->runtime->channels;
	desc.msr = 1;
	while (apcm->substream->runtime->rate > (rsr * desc.msr))
		desc.msr <<= 1;

	desc.mode = MEMRD;
	err = src_mgr->get_src(src_mgr, &desc, (struct src **)&apcm->src);
	if (err)
		goto error1;

	pitch = atc_get_pitch(apcm->substream->runtime->rate, (rsr * desc.msr));
	src = apcm->src;
	src->ops->set_pitch(src, pitch);
	src->ops->set_rom(src, select_rom(pitch));
	src->ops->set_sf(src, convert_format(apcm->substream->runtime->format));
	src->ops->set_pm(src, (src->ops->next_interleave(src) != NULL));
	src->ops->set_bp(src, 1);

	/* Get AMIXER resource */
	n_amixer = (n_amixer < 2) ? 2 : n_amixer;
	apcm->amixers = kzalloc(sizeof(void *)*n_amixer, GFP_KERNEL);
	if (NULL == apcm->amixers) {
		err = -ENOMEM;
		goto error1;
	}
	mix_dsc.msr = desc.msr;
	for (i = 0, apcm->n_amixer = 0; i < n_amixer; i++) {
		err = amixer_mgr->get_amixer(amixer_mgr, &mix_dsc,
					(struct amixer **)&apcm->amixers[i]);
		if (err)
			goto error1;

		apcm->n_amixer++;
	}

	/* Set up device virtual mem map */
	err = ct_map_audio_buffer(atc, apcm);
	if (err < 0)
		goto error1;

	return 0;

error1:
	atc_pcm_release_resources(atc, apcm);
	return err;
}

static int
spdif_passthru_playback_setup(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct dao *dao = container_of(atc->daios[SPDIFOO], struct dao, daio);
	unsigned long flags;
	unsigned int rate = apcm->substream->runtime->rate;
	unsigned int status = 0;
	int err = 0;
	unsigned char iec958_con_fs = 0;

	switch (rate) {
	case 48000:
		iec958_con_fs = IEC958_AES3_CON_FS_48000;
		break;
	case 44100:
		iec958_con_fs = IEC958_AES3_CON_FS_44100;
		break;
	case 32000:
		iec958_con_fs = IEC958_AES3_CON_FS_32000;
		break;
	default:
		return -ENOENT;
	}

	spin_lock_irqsave(&atc->atc_lock, flags);
	dao->ops->get_spos(dao, &status);
	if (((status >> 24) & IEC958_AES3_CON_FS) != iec958_con_fs) {
		status &= ((~IEC958_AES3_CON_FS) << 24);
		status |= (iec958_con_fs << 24);
		dao->ops->set_spos(dao, status);
		dao->ops->commit_write(dao);
	}
	if ((rate != atc->pll_rate) && (32000 != rate)) {
		err = ((struct hw *)atc->hw)->pll_init(atc->hw, rate);
		atc->pll_rate = err ? 0 : rate;
	}
	spin_unlock_irqrestore(&atc->atc_lock, flags);

	return err;
}

static int
spdif_passthru_playback_prepare(struct ct_atc *atc, struct ct_atc_pcm *apcm)
{
	struct src *src = NULL;
	struct amixer *amixer = NULL;
	struct dao *dao = NULL;
	int err = 0;
	int i = 0;
	unsigned long flags;

	if (NULL != apcm->src)
		return 0;

	/* Configure SPDIFOO and PLL to passthrough mode;
	 * determine pll_rate. */
	err = spdif_passthru_playback_setup(atc, apcm);
	if (err)
		return err;

	/* Get needed resources. */
	err = spdif_passthru_playback_get_resources(atc, apcm);
	if (err)
		return err;

	/* Connect resources */
	src = apcm->src;
	for (i = 0; i < apcm->n_amixer; i++) {
		amixer = apcm->amixers[i];
		amixer->ops->setup(amixer, &src->rsc, INIT_VOL, NULL);
		src = src->ops->next_interleave(src);
		if (NULL == src)
			src = apcm->src;
	}
	/* Connect to SPDIFOO */
	spin_lock_irqsave(&atc->atc_lock, flags);
	dao = container_of(atc->daios[SPDIFOO], struct dao, daio);
	amixer = apcm->amixers[0];
	dao->ops->set_left_input(dao, &amixer->rsc);
	amixer = apcm->amixers[1];
	dao->ops->set_right_input(dao, &amixer->rsc);
	spin_unlock_irqrestore(&atc->atc_lock, flags);

	return 0;
}

static int atc_select_line_in(struct ct_atc *atc)
{
	struct hw *hw = atc->hw;
	struct ct_mixer *mixer = atc->mixer;
	struct src *src = NULL;

	if (hw->is_adc_source_selected(hw, ADC_LINEIN))
		return 0;

	mixer->set_input_left(mixer, MIX_MIC_IN, NULL);
	mixer->set_input_right(mixer, MIX_MIC_IN, NULL);

	hw->select_adc_source(hw, ADC_LINEIN);

	src = atc->srcs[2];
	mixer->set_input_left(mixer, MIX_LINE_IN, &src->rsc);
	src = atc->srcs[3];
	mixer->set_input_right(mixer, MIX_LINE_IN, &src->rsc);

	return 0;
}

static int atc_select_mic_in(struct ct_atc *atc)
{
	struct hw *hw = atc->hw;
	struct ct_mixer *mixer = atc->mixer;
	struct src *src = NULL;

	if (hw->is_adc_source_selected(hw, ADC_MICIN))
		return 0;

	mixer->set_input_left(mixer, MIX_LINE_IN, NULL);
	mixer->set_input_right(mixer, MIX_LINE_IN, NULL);

	hw->select_adc_source(hw, ADC_MICIN);

	src = atc->srcs[2];
	mixer->set_input_left(mixer, MIX_MIC_IN, &src->rsc);
	src = atc->srcs[3];
	mixer->set_input_right(mixer, MIX_MIC_IN, &src->rsc);

	return 0;
}

static int atc_have_digit_io_switch(struct ct_atc *atc)
{
	struct hw *hw = atc->hw;

	return hw->have_digit_io_switch(hw);
}

static int atc_select_digit_io(struct ct_atc *atc)
{
	struct hw *hw = atc->hw;

	if (hw->is_adc_source_selected(hw, ADC_NONE))
		return 0;

	hw->select_adc_source(hw, ADC_NONE);

	return 0;
}

static int atc_daio_unmute(struct ct_atc *atc, unsigned char state, int type)
{
	struct daio_mgr *daio_mgr = atc->rsc_mgrs[DAIO];

	if (state)
		daio_mgr->daio_enable(daio_mgr, atc->daios[type]);
	else
		daio_mgr->daio_disable(daio_mgr, atc->daios[type]);

	daio_mgr->commit_write(daio_mgr);

	return 0;
}

static int
atc_dao_get_status(struct ct_atc *atc, unsigned int *status, int type)
{
	struct dao *dao = container_of(atc->daios[type], struct dao, daio);
	return dao->ops->get_spos(dao, status);
}

static int
atc_dao_set_status(struct ct_atc *atc, unsigned int status, int type)
{
	struct dao *dao = container_of(atc->daios[type], struct dao, daio);

	dao->ops->set_spos(dao, status);
	dao->ops->commit_write(dao);
	return 0;
}

static int atc_line_front_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, LINEO1);
}

static int atc_line_surround_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, LINEO4);
}

static int atc_line_clfe_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, LINEO3);
}

static int atc_line_rear_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, LINEO2);
}

static int atc_line_in_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, LINEIM);
}

static int atc_spdif_out_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, SPDIFOO);
}

static int atc_spdif_in_unmute(struct ct_atc *atc, unsigned char state)
{
	return atc_daio_unmute(atc, state, SPDIFIO);
}

static int atc_spdif_out_get_status(struct ct_atc *atc, unsigned int *status)
{
	return atc_dao_get_status(atc, status, SPDIFOO);
}

static int atc_spdif_out_set_status(struct ct_atc *atc, unsigned int status)
{
	return atc_dao_set_status(atc, status, SPDIFOO);
}

static int atc_spdif_out_passthru(struct ct_atc *atc, unsigned char state)
{
	unsigned long flags;
	struct dao_desc da_dsc = {0};
	struct dao *dao = NULL;
	int err = 0;
	struct ct_mixer *mixer = atc->mixer;
	struct rsc *rscs[2] = {NULL};
	unsigned int spos = 0;

	spin_lock_irqsave(&atc->atc_lock, flags);
	dao = container_of(atc->daios[SPDIFOO], struct dao, daio);
	da_dsc.msr = state ? 1 : atc->msr;
	da_dsc.passthru = state ? 1 : 0;
	err = dao->ops->reinit(dao, &da_dsc);
	if (state) {
		spos = IEC958_DEFAULT_CON;
	} else {
		mixer->get_output_ports(mixer, MIX_SPDIF_OUT,
					&rscs[0], &rscs[1]);
		dao->ops->set_left_input(dao, rscs[0]);
		dao->ops->set_right_input(dao, rscs[1]);
		/* Restore PLL to atc->rsr if needed. */
		if (atc->pll_rate != atc->rsr) {
			err = ((struct hw *)atc->hw)->pll_init(atc->hw,
							       atc->rsr);
			atc->pll_rate = err ? 0 : atc->rsr;
		}
	}
	dao->ops->set_spos(dao, spos);
	dao->ops->commit_write(dao);
	spin_unlock_irqrestore(&atc->atc_lock, flags);

	return err;
}

static int ct_atc_destroy(struct ct_atc *atc)
{
	struct daio_mgr *daio_mgr = NULL;
	struct dao *dao = NULL;
	struct dai *dai = NULL;
	struct daio *daio = NULL;
	struct sum_mgr *sum_mgr = NULL;
	struct src_mgr *src_mgr = NULL;
	struct srcimp_mgr *srcimp_mgr = NULL;
	struct srcimp *srcimp = NULL;
	struct ct_mixer *mixer = NULL;
	int i = 0;

	if (NULL == atc)
		return 0;

	/* Stop hardware and disable all interrupts */
	if (NULL != atc->hw)
		((struct hw *)atc->hw)->card_stop(atc->hw);

	/* Destroy internal mixer objects */
	if (NULL != atc->mixer) {
		mixer = atc->mixer;
		mixer->set_input_left(mixer, MIX_LINE_IN, NULL);
		mixer->set_input_right(mixer, MIX_LINE_IN, NULL);
		mixer->set_input_left(mixer, MIX_MIC_IN, NULL);
		mixer->set_input_right(mixer, MIX_MIC_IN, NULL);
		mixer->set_input_left(mixer, MIX_SPDIF_IN, NULL);
		mixer->set_input_right(mixer, MIX_SPDIF_IN, NULL);
		ct_mixer_destroy(atc->mixer);
	}

	if (NULL != atc->daios) {
		daio_mgr = (struct daio_mgr *)atc->rsc_mgrs[DAIO];
		for (i = 0; i < atc->n_daio; i++) {
			daio = atc->daios[i];
			if (daio->type < LINEIM) {
				dao = container_of(daio, struct dao, daio);
				dao->ops->clear_left_input(dao);
				dao->ops->clear_right_input(dao);
			} else {
				dai = container_of(daio, struct dai, daio);
				/* some thing to do for dai ... */
			}
			daio_mgr->put_daio(daio_mgr, daio);
		}
		kfree(atc->daios);
	}

	if (NULL != atc->pcm) {
		sum_mgr = atc->rsc_mgrs[SUM];
		for (i = 0; i < atc->n_pcm; i++)
			sum_mgr->put_sum(sum_mgr, atc->pcm[i]);

		kfree(atc->pcm);
	}

	if (NULL != atc->srcs) {
		src_mgr = atc->rsc_mgrs[SRC];
		for (i = 0; i < atc->n_src; i++)
			src_mgr->put_src(src_mgr, atc->srcs[i]);

		kfree(atc->srcs);
	}

	if (NULL != atc->srcimps) {
		srcimp_mgr = atc->rsc_mgrs[SRCIMP];
		for (i = 0; i < atc->n_srcimp; i++) {
			srcimp = atc->srcimps[i];
			srcimp->ops->unmap(srcimp);
			srcimp_mgr->put_srcimp(srcimp_mgr, atc->srcimps[i]);
		}
		kfree(atc->srcimps);
	}

	for (i = 0; i < NUM_RSCTYP; i++) {
		if ((NULL != rsc_mgr_funcs[i].destroy) &&
		    (NULL != atc->rsc_mgrs[i]))
			rsc_mgr_funcs[i].destroy(atc->rsc_mgrs[i]);

	}

	if (NULL != atc->hw)
		destroy_hw_obj((struct hw *)atc->hw);

	/* Destroy device virtual memory manager object */
	if (NULL != atc->vm) {
		ct_vm_destroy(atc->vm);
		atc->vm = NULL;
	}

	kfree(atc);

	return 0;
}

static int atc_dev_free(struct snd_device *dev)
{
	struct ct_atc *atc = dev->device_data;
	return ct_atc_destroy(atc);
}

static int atc_identify_card(struct ct_atc *atc)
{
	u16 subsys = 0;
	u8 revision = 0;
	struct pci_dev *pci = atc->pci;
	const struct ct_atc_chip_details *d;
	enum CTCARDS i;

	pci_read_config_word(pci, PCI_SUBSYSTEM_ID, &subsys);
	pci_read_config_byte(pci, PCI_REVISION_ID, &revision);
	atc->chip_details = NULL;
	atc->model = NUM_CTCARDS;
	for (d = atc_chip_details; d->vendor; d++) {
		if (d->vendor != pci->vendor || d->device != pci->device)
			continue;

		if (NULL == d->sub_details) {
			atc->chip_details = d;
			break;
		}
		for (i = 0; i < NUM_CTCARDS; i++) {
			if ((d->sub_details[i].subsys == subsys) ||
			    (((subsys & 0x6000) == 0x6000) &&
			    ((d->sub_details[i].subsys & 0x6000) == 0x6000))) {
				atc->model = i;
				break;
			}
		}
		if (i >= NUM_CTCARDS)
			continue;

		atc->chip_details = d;
		break;
		/* not take revision into consideration now */
	}
	if (!d->vendor)
		return -ENOENT;

	return 0;
}

static int ct_create_alsa_devs(struct ct_atc *atc)
{
	enum CTALSADEVS i;
	struct hw *hw = atc->hw;
	int err;

	switch (hw->get_chip_type(hw)) {
	case ATC20K1:
		alsa_dev_funcs[MIXER].public_name = "20K1";
		break;
	case ATC20K2:
		alsa_dev_funcs[MIXER].public_name = "20K2";
		break;
	default:
		alsa_dev_funcs[MIXER].public_name = "Unknown";
		break;
	}

	for (i = 0; i < NUM_CTALSADEVS; i++) {
		if (NULL == alsa_dev_funcs[i].create)
			continue;

		err = alsa_dev_funcs[i].create(atc, i,
				alsa_dev_funcs[i].public_name);
		if (err) {
			printk(KERN_ERR "Creating alsa device %d failed!\n", i);
			return err;
		}
	}

	return 0;
}

static int atc_create_hw_devs(struct ct_atc *atc)
{
	struct hw *hw = NULL;
	struct card_conf info = {0};
	int i = 0, err = 0;

	err = create_hw_obj(atc->pci, &hw);
	if (err) {
		printk(KERN_ERR "Failed to create hw obj!!!\n");
		return err;
	}
	atc->hw = hw;

	/* Initialize card hardware. */
	info.rsr = atc->rsr;
	info.msr = atc->msr;
	info.vm_pgt_phys = atc_get_ptp_phys(atc, 0);
	err = hw->card_init(hw, &info);
	if (err < 0)
		return err;

	for (i = 0; i < NUM_RSCTYP; i++) {
		if (NULL == rsc_mgr_funcs[i].create)
			continue;

		err = rsc_mgr_funcs[i].create(atc->hw, &atc->rsc_mgrs[i]);
		if (err) {
			printk(KERN_ERR "Failed to create rsc_mgr %d!!!\n", i);
			return err;
		}
	}

	return 0;
}

static int atc_get_resources(struct ct_atc *atc)
{
	struct daio_desc da_desc = {0};
	struct daio_mgr *daio_mgr = NULL;
	struct src_desc src_dsc = {0};
	struct src_mgr *src_mgr = NULL;
	struct srcimp_desc srcimp_dsc = {0};
	struct srcimp_mgr *srcimp_mgr = NULL;
	struct sum_desc sum_dsc = {0};
	struct sum_mgr *sum_mgr = NULL;
	int err = 0, i = 0;
	unsigned short subsys_id = 0;

	atc->daios = kzalloc(sizeof(void *)*(DAIONUM), GFP_KERNEL);
	if (NULL == atc->daios)
		return -ENOMEM;

	atc->srcs = kzalloc(sizeof(void *)*(2*2), GFP_KERNEL);
	if (NULL == atc->srcs)
		return -ENOMEM;

	atc->srcimps = kzalloc(sizeof(void *)*(2*2), GFP_KERNEL);
	if (NULL == atc->srcimps)
		return -ENOMEM;

	atc->pcm = kzalloc(sizeof(void *)*(2*4), GFP_KERNEL);
	if (NULL == atc->pcm)
		return -ENOMEM;

	daio_mgr = (struct daio_mgr *)atc->rsc_mgrs[DAIO];
	da_desc.msr = atc->msr;
	for (i = 0, atc->n_daio = 0; i < DAIONUM-1; i++) {
		da_desc.type = i;
		err = daio_mgr->get_daio(daio_mgr, &da_desc,
					(struct daio **)&atc->daios[i]);
		if (err) {
			printk(KERN_ERR "Failed to get DAIO "
					"resource %d!!!\n", i);
			return err;
		}
		atc->n_daio++;
	}
	pci_read_config_word(atc->pci, PCI_SUBSYSTEM_ID, &subsys_id);
	if ((subsys_id == 0x0029) || (subsys_id == 0x0031)) {
		/* SB073x cards */
		da_desc.type = SPDIFI1;
	} else {
		da_desc.type = SPDIFIO;
	}
	err = daio_mgr->get_daio(daio_mgr, &da_desc,
				(struct daio **)&atc->daios[i]);
	if (err) {
		printk(KERN_ERR "Failed to get S/PDIF-in resource!!!\n");
		return err;
	}
	atc->n_daio++;

	src_mgr = atc->rsc_mgrs[SRC];
	src_dsc.multi = 1;
	src_dsc.msr = atc->msr;
	src_dsc.mode = ARCRW;
	for (i = 0, atc->n_src = 0; i < (2*2); i++) {
		err = src_mgr->get_src(src_mgr, &src_dsc,
					(struct src **)&atc->srcs[i]);
		if (err)
			return err;

		atc->n_src++;
	}

	srcimp_mgr = atc->rsc_mgrs[SRCIMP];
	srcimp_dsc.msr = 8; /* SRCIMPs for S/PDIFIn SRT */
	for (i = 0, atc->n_srcimp = 0; i < (2*1); i++) {
		err = srcimp_mgr->get_srcimp(srcimp_mgr, &srcimp_dsc,
					(struct srcimp **)&atc->srcimps[i]);
		if (err)
			return err;

		atc->n_srcimp++;
	}
	srcimp_dsc.msr = 8; /* SRCIMPs for LINE/MICIn SRT */
	for (i = 0; i < (2*1); i++) {
		err = srcimp_mgr->get_srcimp(srcimp_mgr, &srcimp_dsc,
				(struct srcimp **)&atc->srcimps[2*1+i]);
		if (err)
			return err;

		atc->n_srcimp++;
	}

	sum_mgr = atc->rsc_mgrs[SUM];
	sum_dsc.msr = atc->msr;
	for (i = 0, atc->n_pcm = 0; i < (2*4); i++) {
		err = sum_mgr->get_sum(sum_mgr, &sum_dsc,
					(struct sum **)&atc->pcm[i]);
		if (err)
			return err;

		atc->n_pcm++;
	}

	err = ct_mixer_create(atc, (struct ct_mixer **)&atc->mixer);
	if (err) {
		printk(KERN_ERR "Failed to create mixer obj!!!\n");
		return err;
	}

	return 0;
}

static void
atc_connect_dai(struct src_mgr *src_mgr, struct dai *dai,
		struct src **srcs, struct srcimp **srcimps)
{
	struct rsc *rscs[2] = {NULL};
	struct src *src = NULL;
	struct srcimp *srcimp = NULL;
	int i = 0;

	rscs[0] = &dai->daio.rscl;
	rscs[1] = &dai->daio.rscr;
	for (i = 0; i < 2; i++) {
		src = srcs[i];
		srcimp = srcimps[i];
		srcimp->ops->map(srcimp, src, rscs[i]);
		src_mgr->src_disable(src_mgr, src);
	}

	src_mgr->commit_write(src_mgr); /* Actually disable SRCs */

	src = srcs[0];
	src->ops->set_pm(src, 1);
	for (i = 0; i < 2; i++) {
		src = srcs[i];
		src->ops->set_state(src, SRC_STATE_RUN);
		src->ops->commit_write(src);
		src_mgr->src_enable_s(src_mgr, src);
	}

	dai->ops->set_srt_srcl(dai, &(srcs[0]->rsc));
	dai->ops->set_srt_srcr(dai, &(srcs[1]->rsc));

	dai->ops->set_enb_src(dai, 1);
	dai->ops->set_enb_srt(dai, 1);
	dai->ops->commit_write(dai);

	src_mgr->commit_write(src_mgr); /* Synchronously enable SRCs */
}

static void atc_connect_resources(struct ct_atc *atc)
{
	struct dai *dai = NULL;
	struct dao *dao = NULL;
	struct src *src = NULL;
	struct sum *sum = NULL;
	struct ct_mixer *mixer = NULL;
	struct rsc *rscs[2] = {NULL};
	int i = 0, j = 0;

	mixer = atc->mixer;

	for (i = MIX_WAVE_FRONT, j = LINEO1; i <= MIX_SPDIF_OUT; i++, j++) {
		mixer->get_output_ports(mixer, i, &rscs[0], &rscs[1]);
		dao = container_of(atc->daios[j], struct dao, daio);
		dao->ops->set_left_input(dao, rscs[0]);
		dao->ops->set_right_input(dao, rscs[1]);
	}

	dai = container_of(atc->daios[LINEIM], struct dai, daio);
	atc_connect_dai(atc->rsc_mgrs[SRC], dai,
			(struct src **)&atc->srcs[2],
			(struct srcimp **)&atc->srcimps[2]);
	src = atc->srcs[2];
	mixer->set_input_left(mixer, MIX_LINE_IN, &src->rsc);
	src = atc->srcs[3];
	mixer->set_input_right(mixer, MIX_LINE_IN, &src->rsc);

	dai = container_of(atc->daios[SPDIFIO], struct dai, daio);
	atc_connect_dai(atc->rsc_mgrs[SRC], dai,
			(struct src **)&atc->srcs[0],
			(struct srcimp **)&atc->srcimps[0]);

	src = atc->srcs[0];
	mixer->set_input_left(mixer, MIX_SPDIF_IN, &src->rsc);
	src = atc->srcs[1];
	mixer->set_input_right(mixer, MIX_SPDIF_IN, &src->rsc);

	for (i = MIX_PCMI_FRONT, j = 0; i <= MIX_PCMI_SURROUND; i++, j += 2) {
		sum = atc->pcm[j];
		mixer->set_input_left(mixer, i, &sum->rsc);
		sum = atc->pcm[j+1];
		mixer->set_input_right(mixer, i, &sum->rsc);
	}
}

static void atc_set_ops(struct ct_atc *atc)
{
	/* Set operations */
	atc->map_audio_buffer = ct_map_audio_buffer;
	atc->unmap_audio_buffer = ct_unmap_audio_buffer;
	atc->pcm_playback_prepare = atc_pcm_playback_prepare;
	atc->pcm_release_resources = atc_pcm_release_resources;
	atc->pcm_playback_start = atc_pcm_playback_start;
	atc->pcm_playback_stop = atc_pcm_stop;
	atc->pcm_playback_position = atc_pcm_playback_position;
	atc->pcm_capture_prepare = atc_pcm_capture_prepare;
	atc->pcm_capture_start = atc_pcm_capture_start;
	atc->pcm_capture_stop = atc_pcm_stop;
	atc->pcm_capture_position = atc_pcm_capture_position;
	atc->spdif_passthru_playback_prepare = spdif_passthru_playback_prepare;
	atc->get_ptp_phys = atc_get_ptp_phys;
	atc->select_line_in = atc_select_line_in;
	atc->select_mic_in = atc_select_mic_in;
	atc->select_digit_io = atc_select_digit_io;
	atc->line_front_unmute = atc_line_front_unmute;
	atc->line_surround_unmute = atc_line_surround_unmute;
	atc->line_clfe_unmute = atc_line_clfe_unmute;
	atc->line_rear_unmute = atc_line_rear_unmute;
	atc->line_in_unmute = atc_line_in_unmute;
	atc->spdif_out_unmute = atc_spdif_out_unmute;
	atc->spdif_in_unmute = atc_spdif_in_unmute;
	atc->spdif_out_get_status = atc_spdif_out_get_status;
	atc->spdif_out_set_status = atc_spdif_out_set_status;
	atc->spdif_out_passthru = atc_spdif_out_passthru;
	atc->have_digit_io_switch = atc_have_digit_io_switch;
}

/**
 *  ct_atc_create - create and initialize a hardware manager
 *  @card: corresponding alsa card object
 *  @pci: corresponding kernel pci device object
 *  @ratc: return created object address in it
 *
 *  Creates and initializes a hardware manager.
 *
 *  Creates kmallocated ct_atc structure. Initializes hardware.
 *  Returns 0 if suceeds, or negative error code if fails.
 */

int ct_atc_create(struct snd_card *card, struct pci_dev *pci,
		  unsigned int rsr, unsigned int msr, struct ct_atc **ratc)
{
	struct ct_atc *atc = NULL;
	static struct snd_device_ops ops = {
		.dev_free = atc_dev_free,
	};
	int err = 0;

	*ratc = NULL;

	atc = kzalloc(sizeof(*atc), GFP_KERNEL);
	if (NULL == atc)
		return -ENOMEM;

	atc->card = card;
	atc->pci = pci;
	atc->rsr = rsr;
	atc->msr = msr;

	/* Set operations */
	atc_set_ops(atc);

	spin_lock_init(&atc->atc_lock);
	spin_lock_init(&atc->vm_lock);

	/* Find card model */
	err = atc_identify_card(atc);
	if (err < 0) {
		printk(KERN_ERR "ctatc: Card not recognised\n");
		goto error1;
	}

	/* Set up device virtual memory management object */
	err = ct_vm_create(&atc->vm);
	if (err < 0)
		goto error1;

	/* Create all atc hw devices */
	err = atc_create_hw_devs(atc);
	if (err < 0)
		goto error1;

	/* Get resources */
	err = atc_get_resources(atc);
	if (err < 0)
		goto error1;

	/* Build topology */
	atc_connect_resources(atc);

	atc->create_alsa_devs = ct_create_alsa_devs;

	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, atc, &ops);
	if (err < 0)
		goto error1;

	snd_card_set_dev(card, &pci->dev);

	*ratc = atc;
	return 0;

error1:
	ct_atc_destroy(atc);
	printk(KERN_ERR "Something wrong!!!\n");
	return err;
}

