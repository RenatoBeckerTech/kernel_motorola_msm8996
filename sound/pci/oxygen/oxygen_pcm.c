/*
 * C-Media CMI8788 driver - PCM code
 *
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 *
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this driver; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <sound/driver.h>
#include <linux/pci.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "oxygen.h"

static struct snd_pcm_hardware oxygen_hardware[PCM_COUNT] = {
	[PCM_A] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
		.rates = SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_192000,
		.rate_min = 44100,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 2,
		.buffer_bytes_max = 256 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 128 * 1024,
		.periods_min = 2,
		.periods_max = 2048,
	},
	[PCM_B] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
		.rates = SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_64000 |
			 SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000,
		.rate_min = 32000,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 2,
		.buffer_bytes_max = 256 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 128 * 1024,
		.periods_min = 2,
		.periods_max = 2048,
	},
	[PCM_C] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
		.rates = SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000,
		.rate_min = 44100,
		.rate_max = 96000,
		.channels_min = 2,
		.channels_max = 2,
		.buffer_bytes_max = 256 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 128 * 1024,
		.periods_min = 2,
		.periods_max = 2048,
	},
	[PCM_SPDIF] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
		.rates = SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_64000 |
			 SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000,
		.rate_min = 32000,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 2,
		.buffer_bytes_max = 256 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 128 * 1024,
		.periods_min = 2,
		.periods_max = 2048,
	},
	[PCM_MULTICH] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
		.rates = SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_44100 |
			 SNDRV_PCM_RATE_48000 |
			 SNDRV_PCM_RATE_64000 |
			 SNDRV_PCM_RATE_88200 |
			 SNDRV_PCM_RATE_96000 |
			 SNDRV_PCM_RATE_176400 |
			 SNDRV_PCM_RATE_192000,
		.rate_min = 32000,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 8,
		.buffer_bytes_max = 2048 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 256 * 1024,
		.periods_min = 2,
		.periods_max = 16384,
	},
	[PCM_AC97] = {
		.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_SYNC_START,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rates = SNDRV_PCM_RATE_48000,
		.rate_min = 48000,
		.rate_max = 48000,
		.channels_min = 2,
		.channels_max = 2,
		.buffer_bytes_max = 256 * 1024,
		.period_bytes_min = 128,
		.period_bytes_max = 128 * 1024,
		.periods_min = 2,
		.periods_max = 2048,
	},
};

static int oxygen_open(struct snd_pcm_substream *substream,
		       unsigned int channel)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err;

	runtime->private_data = (void *)channel;
	runtime->hw = oxygen_hardware[channel];
	err = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (err < 0)
		return err;
	err = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (err < 0)
		return err;
	if (runtime->hw.formats & SNDRV_PCM_FMTBIT_S32_LE) {
		err = snd_pcm_hw_constraint_msbits(runtime, 0, 32, 24);
		if (err < 0)
			return err;
	}
	if (runtime->hw.channels_max > 2) {
		err = snd_pcm_hw_constraint_step(runtime, 0,
						 SNDRV_PCM_HW_PARAM_CHANNELS,
						 2);
		if (err < 0)
			return err;
	}
	snd_pcm_set_sync(substream);
	chip->streams[channel] = substream;

	mutex_lock(&chip->mutex);
	chip->pcm_active |= 1 << channel;
	if (channel == PCM_SPDIF) {
		chip->spdif_pcm_bits = chip->spdif_bits;
		chip->spdif_pcm_ctl->vd[0].access &=
			~SNDRV_CTL_ELEM_ACCESS_INACTIVE;
		snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE |
			       SNDRV_CTL_EVENT_MASK_INFO,
			       &chip->spdif_pcm_ctl->id);
	}
	mutex_unlock(&chip->mutex);

	return 0;
}

static int oxygen_rec_a_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_A);
}

static int oxygen_rec_b_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_B);
}

static int oxygen_rec_c_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_C);
}

static int oxygen_spdif_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_SPDIF);
}

static int oxygen_multich_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_MULTICH);
}

static int oxygen_ac97_open(struct snd_pcm_substream *substream)
{
	return oxygen_open(substream, PCM_AC97);
}

static int oxygen_close(struct snd_pcm_substream *substream)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	unsigned int channel = (unsigned int)substream->runtime->private_data;

	mutex_lock(&chip->mutex);
	chip->pcm_active &= ~(1 << channel);
	if (channel == PCM_SPDIF) {
		chip->spdif_pcm_ctl->vd[0].access |=
			SNDRV_CTL_ELEM_ACCESS_INACTIVE;
		snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE |
			       SNDRV_CTL_EVENT_MASK_INFO,
			       &chip->spdif_pcm_ctl->id);
	}
	if (channel == PCM_SPDIF || channel == PCM_MULTICH)
		oxygen_update_spdif_source(chip);
	mutex_unlock(&chip->mutex);

	chip->streams[channel] = NULL;
	return 0;
}

static unsigned int oxygen_format(struct snd_pcm_hw_params *hw_params)
{
	if (params_format(hw_params) == SNDRV_PCM_FORMAT_S32_LE)
		return OXYGEN_FORMAT_24;
	else
		return OXYGEN_FORMAT_16;
}

static unsigned int oxygen_rate(struct snd_pcm_hw_params *hw_params)
{
	switch (params_rate(hw_params)) {
	case 32000:
		return OXYGEN_RATE_32000;
	case 44100:
		return OXYGEN_RATE_44100;
	default: /* 48000 */
		return OXYGEN_RATE_48000;
	case 64000:
		return OXYGEN_RATE_64000;
	case 88200:
		return OXYGEN_RATE_88200;
	case 96000:
		return OXYGEN_RATE_96000;
	case 176400:
		return OXYGEN_RATE_176400;
	case 192000:
		return OXYGEN_RATE_192000;
	}
}

static unsigned int oxygen_i2s_magic2(struct snd_pcm_hw_params *hw_params)
{
	return params_rate(hw_params) <= 96000 ? 0x10 : 0x00;
}

static unsigned int oxygen_i2s_format(struct snd_pcm_hw_params *hw_params)
{
	if (params_format(hw_params) == SNDRV_PCM_FORMAT_S32_LE)
		return OXYGEN_I2S_FORMAT_24;
	else
		return OXYGEN_I2S_FORMAT_16;
}

static unsigned int oxygen_play_channels(struct snd_pcm_hw_params *hw_params)
{
	switch (params_channels(hw_params)) {
	default: /* 2 */
		return OXYGEN_PLAY_CHANNELS_2;
	case 4:
		return OXYGEN_PLAY_CHANNELS_4;
	case 6:
		return OXYGEN_PLAY_CHANNELS_6;
	case 8:
		return OXYGEN_PLAY_CHANNELS_8;
	}
}

static const unsigned int channel_base_registers[PCM_COUNT] = {
	[PCM_A] = OXYGEN_DMA_A_ADDRESS,
	[PCM_B] = OXYGEN_DMA_B_ADDRESS,
	[PCM_C] = OXYGEN_DMA_C_ADDRESS,
	[PCM_SPDIF] = OXYGEN_DMA_SPDIF_ADDRESS,
	[PCM_MULTICH] = OXYGEN_DMA_MULTICH_ADDRESS,
	[PCM_AC97] = OXYGEN_DMA_AC97_ADDRESS,
};

static int oxygen_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	unsigned int channel = (unsigned int)substream->runtime->private_data;
	int err;

	err = snd_pcm_lib_malloc_pages(substream,
				       params_buffer_bytes(hw_params));
	if (err < 0)
		return err;

	oxygen_write32(chip, channel_base_registers[channel],
		       (u32)substream->runtime->dma_addr);
	if (channel == PCM_MULTICH) {
		oxygen_write32(chip, OXYGEN_DMA_MULTICH_COUNT,
			       params_buffer_bytes(hw_params) / 4 - 1);
		oxygen_write32(chip, OXYGEN_DMA_MULTICH_TCOUNT,
			       params_period_bytes(hw_params) / 4 - 1);
	} else {
		oxygen_write16(chip, channel_base_registers[channel] + 4,
			       params_buffer_bytes(hw_params) / 4 - 1);
		oxygen_write16(chip, channel_base_registers[channel] + 6,
			       params_period_bytes(hw_params) / 4 - 1);
	}
	return 0;
}

static int oxygen_rec_a_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8_masked(chip, OXYGEN_REC_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_REC_FORMAT_A_SHIFT,
			     OXYGEN_REC_FORMAT_A_MASK);
	oxygen_write8_masked(chip, OXYGEN_I2S_A_FORMAT,
			     oxygen_rate(hw_params) |
			     oxygen_i2s_magic2(hw_params) |
			     oxygen_i2s_format(hw_params),
			     OXYGEN_I2S_RATE_MASK |
			     OXYGEN_I2S_MAGIC2_MASK |
			     OXYGEN_I2S_FORMAT_MASK);
	oxygen_clear_bits8(chip, OXYGEN_REC_ROUTING, 0x08);
	spin_unlock_irq(&chip->reg_lock);

	mutex_lock(&chip->mutex);
	chip->model->set_adc_params(chip, hw_params);
	mutex_unlock(&chip->mutex);
	return 0;
}

static int oxygen_rec_b_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8_masked(chip, OXYGEN_REC_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_REC_FORMAT_B_SHIFT,
			     OXYGEN_REC_FORMAT_B_MASK);
	oxygen_write8_masked(chip, OXYGEN_I2S_B_FORMAT,
			     oxygen_rate(hw_params) |
			     oxygen_i2s_magic2(hw_params) |
			     oxygen_i2s_format(hw_params),
			     OXYGEN_I2S_RATE_MASK |
			     OXYGEN_I2S_MAGIC2_MASK |
			     OXYGEN_I2S_FORMAT_MASK);
	oxygen_clear_bits8(chip, OXYGEN_REC_ROUTING, 0x10);
	spin_unlock_irq(&chip->reg_lock);

	mutex_lock(&chip->mutex);
	chip->model->set_adc_params(chip, hw_params);
	mutex_unlock(&chip->mutex);
	return 0;
}

static int oxygen_rec_c_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8_masked(chip, OXYGEN_REC_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_REC_FORMAT_C_SHIFT,
			     OXYGEN_REC_FORMAT_C_MASK);
	oxygen_clear_bits8(chip, OXYGEN_REC_ROUTING, 0x20);
	spin_unlock_irq(&chip->reg_lock);
	return 0;
}

static int oxygen_spdif_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_clear_bits32(chip, OXYGEN_SPDIF_CONTROL,
			    OXYGEN_SPDIF_OUT_ENABLE);
	oxygen_write8_masked(chip, OXYGEN_PLAY_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_SPDIF_FORMAT_SHIFT,
			     OXYGEN_SPDIF_FORMAT_MASK);
	oxygen_write32_masked(chip, OXYGEN_SPDIF_CONTROL,
			      oxygen_rate(hw_params) << OXYGEN_SPDIF_OUT_RATE_SHIFT,
			      OXYGEN_SPDIF_OUT_RATE_MASK);
	oxygen_update_spdif_source(chip);
	spin_unlock_irq(&chip->reg_lock);
	return 0;
}

static int oxygen_multich_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8_masked(chip, OXYGEN_PLAY_CHANNELS,
			     oxygen_play_channels(hw_params),
			     OXYGEN_PLAY_CHANNELS_MASK);
	oxygen_write8_masked(chip, OXYGEN_PLAY_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_MULTICH_FORMAT_SHIFT,
			     OXYGEN_MULTICH_FORMAT_MASK);
	oxygen_write16_masked(chip, OXYGEN_I2S_MULTICH_FORMAT,
			      oxygen_rate(hw_params) | oxygen_i2s_format(hw_params),
			      OXYGEN_I2S_RATE_MASK | OXYGEN_I2S_FORMAT_MASK);
	oxygen_clear_bits16(chip, OXYGEN_PLAY_ROUTING, 0x001f);
	oxygen_update_dac_routing(chip);
	oxygen_update_spdif_source(chip);
	spin_unlock_irq(&chip->reg_lock);

	mutex_lock(&chip->mutex);
	chip->model->set_dac_params(chip, hw_params);
	mutex_unlock(&chip->mutex);
	return 0;
}

static int oxygen_ac97_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	int err;

	err = oxygen_hw_params(substream, hw_params);
	if (err < 0)
		return err;

	spin_lock_irq(&chip->reg_lock);
	oxygen_write8_masked(chip, OXYGEN_PLAY_FORMAT,
			     oxygen_format(hw_params) << OXYGEN_AC97_FORMAT_SHIFT,
			     OXYGEN_AC97_FORMAT_MASK);
	spin_unlock_irq(&chip->reg_lock);
	return 0;
}

static int oxygen_hw_free(struct snd_pcm_substream *substream)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	unsigned int channel = (unsigned int)substream->runtime->private_data;

	spin_lock_irq(&chip->reg_lock);
	chip->interrupt_mask &= ~(1 << channel);
	oxygen_write16(chip, OXYGEN_INTERRUPT_MASK, chip->interrupt_mask);
	spin_unlock_irq(&chip->reg_lock);

	return snd_pcm_lib_free_pages(substream);
}

static int oxygen_spdif_hw_free(struct snd_pcm_substream *substream)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);

	spin_lock_irq(&chip->reg_lock);
	oxygen_clear_bits32(chip, OXYGEN_SPDIF_CONTROL,
			    OXYGEN_SPDIF_OUT_ENABLE);
	spin_unlock_irq(&chip->reg_lock);
	return oxygen_hw_free(substream);
}

static int oxygen_prepare(struct snd_pcm_substream *substream)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	unsigned int channel = (unsigned int)substream->runtime->private_data;
	unsigned int channel_mask = 1 << channel;

	spin_lock_irq(&chip->reg_lock);
	oxygen_set_bits8(chip, OXYGEN_DMA_FLUSH, channel_mask);
	oxygen_clear_bits8(chip, OXYGEN_DMA_FLUSH, channel_mask);

	chip->interrupt_mask |= channel_mask;
	oxygen_write16(chip, OXYGEN_INTERRUPT_MASK, chip->interrupt_mask);
	spin_unlock_irq(&chip->reg_lock);
	return 0;
}

static int oxygen_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_substream *s;
	unsigned int mask = 0;
	int running;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		running = 0;
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		running = 1;
		break;
	default:
		return -EINVAL;
	}

	snd_pcm_group_for_each_entry(s, substream) {
		if (snd_pcm_substream_chip(s) == chip) {
			mask |= 1 << (unsigned int)s->runtime->private_data;
			snd_pcm_trigger_done(s, substream);
		}
	}

	spin_lock(&chip->reg_lock);
	if (running)
		chip->pcm_running |= mask;
	else
		chip->pcm_running &= ~mask;
	oxygen_write8(chip, OXYGEN_DMA_STATUS, chip->pcm_running);
	spin_unlock(&chip->reg_lock);
	return 0;
}

static snd_pcm_uframes_t oxygen_pointer(struct snd_pcm_substream *substream)
{
	struct oxygen *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int channel = (unsigned int)runtime->private_data;
	u32 curr_addr;

	/* no spinlock, this read should be atomic */
	curr_addr = oxygen_read32(chip, channel_base_registers[channel]);
	return bytes_to_frames(runtime, curr_addr - (u32)runtime->dma_addr);
}

static struct snd_pcm_ops oxygen_rec_a_ops = {
	.open      = oxygen_rec_a_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_rec_a_hw_params,
	.hw_free   = oxygen_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static struct snd_pcm_ops oxygen_rec_b_ops = {
	.open      = oxygen_rec_b_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_rec_b_hw_params,
	.hw_free   = oxygen_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static struct snd_pcm_ops oxygen_rec_c_ops = {
	.open      = oxygen_rec_c_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_rec_c_hw_params,
	.hw_free   = oxygen_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static struct snd_pcm_ops oxygen_spdif_ops = {
	.open      = oxygen_spdif_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_spdif_hw_params,
	.hw_free   = oxygen_spdif_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static struct snd_pcm_ops oxygen_multich_ops = {
	.open      = oxygen_multich_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_multich_hw_params,
	.hw_free   = oxygen_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static struct snd_pcm_ops oxygen_ac97_ops = {
	.open      = oxygen_ac97_open,
	.close     = oxygen_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = oxygen_ac97_hw_params,
	.hw_free   = oxygen_hw_free,
	.prepare   = oxygen_prepare,
	.trigger   = oxygen_trigger,
	.pointer   = oxygen_pointer,
};

static void oxygen_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

int __devinit oxygen_pcm_init(struct oxygen *chip)
{
	struct snd_pcm *pcm;
	int err;

	err = snd_pcm_new(chip->card, "Analog", 0, 1, 1, &pcm);
	if (err < 0)
		return err;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &oxygen_multich_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			chip->model->record_from_dma_b ?
			&oxygen_rec_b_ops : &oxygen_rec_a_ops);
	pcm->private_data = chip;
	pcm->private_free = oxygen_pcm_free;
	strcpy(pcm->name, "Analog");
	snd_pcm_lib_preallocate_pages(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream,
				      SNDRV_DMA_TYPE_DEV,
				      snd_dma_pci_data(chip->pci),
				      512 * 1024, 2048 * 1024);
	snd_pcm_lib_preallocate_pages(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream,
				      SNDRV_DMA_TYPE_DEV,
				      snd_dma_pci_data(chip->pci),
				      128 * 1024, 256 * 1024);

	err = snd_pcm_new(chip->card, "Digital", 1, 1, 1, &pcm);
	if (err < 0)
		return err;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &oxygen_spdif_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &oxygen_rec_c_ops);
	pcm->private_data = chip;
	pcm->private_free = oxygen_pcm_free;
	strcpy(pcm->name, "Digital");
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(chip->pci),
					      128 * 1024, 256 * 1024);

	if (chip->has_2nd_ac97_codec) {
		err = snd_pcm_new(chip->card, "AC97", 2, 1, 0, &pcm);
		if (err < 0)
			return err;
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				&oxygen_ac97_ops);
		pcm->private_data = chip;
		pcm->private_free = oxygen_pcm_free;
		strcpy(pcm->name, "Front Panel");
		snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						      snd_dma_pci_data(chip->pci),
						      128 * 1024, 256 * 1024);
	}
	return 0;
}
