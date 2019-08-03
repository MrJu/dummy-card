/*
 * Copyright (C) 2019 Andrew <mrju.email@gail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define STR(x) _STR(x)
#define _STR(x) #x

#define VERSION_PREFIX Dummy-Asoc
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 0

#define VERSION STR(VERSION_PREFIX-MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION)

#define CODEC_STEREO_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#define CODEC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

#define DEVICE_NAME "dummy_codec"

static int codec_probe(struct snd_soc_component *codec)
{
	return 0;
}

static void codec_remove(struct snd_soc_component *codec)
{
	return;
}

static int codec_suspend(struct snd_soc_component *codec)
{
	return 0;
}

static int codec_resume(struct snd_soc_component *codec)
{
	return 0;
}

static int codec_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static int codec_set_dai_sysclk(struct snd_soc_dai *dai,
			int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int codec_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	return 0;
}

static int codec_set_bias_level(struct snd_soc_component *codec,
			enum snd_soc_bias_level level)
{
	return 0;
}

static int codec_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	return 0;
}

static int codec_pcm_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	return 0;
}

static void codec_pcm_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	return;
}

static int codec_pcm_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	return 0;
}

static const struct snd_soc_dai_ops codec_dai_ops = {
	.startup = codec_pcm_startup,
	.shutdown = codec_pcm_shutdown,
	.prepare = codec_pcm_prepare,
	.hw_params = codec_hw_params,
	.set_fmt = codec_set_dai_fmt,
	.set_sysclk = codec_set_dai_sysclk,
	.set_bclk_ratio = codec_set_bclk_ratio,
};

static struct snd_soc_dai_driver codec_dai[] = {
	{
		.name = "codec.dai.i2s", // seems to not to be used for matching
		.id = 0,
		.playback = {
			.stream_name = "i2s Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CODEC_STEREO_RATES,
			.formats = CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "i2s Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CODEC_STEREO_RATES,
			.formats = CODEC_FORMATS,
		},
		.ops = &codec_dai_ops,
		.symmetric_rates = 1,
	},
};

static struct snd_soc_component_driver soc_codec_driver = {
	.probe = codec_probe,
	.remove = codec_remove,
	.suspend = codec_suspend,
	.resume = codec_resume,
	.set_bias_level = codec_set_bias_level,
};

int snd_codec_probe(struct platform_device *pdev)
{
	return devm_snd_soc_register_component(&pdev->dev,
			&soc_codec_driver, codec_dai, ARRAY_SIZE(codec_dai));
}

static int snd_codec_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct platform_device_id dummy_codec_platform_id[] = {
	{
		.name = DEVICE_NAME,
		.driver_data = 0,
	},
	{
		/* end */
	},

};
MODULE_DEVICE_TABLE(platform, dummy_codec_platform_id);

static const struct of_device_id dummy_codec_of_match[] = {
	{
		.compatible = "artech,dummy_codec",
	},
	{
		/* end */
	},
};
MODULE_DEVICE_TABLE(of, dummy_codec_of_match);

static struct platform_driver dummy_codec_drv = {
	.probe  = snd_codec_probe,
	.remove = snd_codec_remove,
	.id_table = dummy_codec_platform_id,
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = of_match_ptr(dummy_codec_of_match),
	}
};

int __init dummy_codec_init(void)
{
	return platform_driver_register(&dummy_codec_drv);

}

void __exit dummy_codec_exit(void)
{
	platform_driver_unregister(&dummy_codec_drv);
}

module_init(dummy_codec_init);
module_exit(dummy_codec_exit);

MODULE_ALIAS("dummy-asoc");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Linux is not Unix");
MODULE_AUTHOR("andrew, mrju.email@gmail.com");
