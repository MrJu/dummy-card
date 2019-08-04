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

#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define STR(x) _STR(x)
#define _STR(x) #x

#define VERSION_PREFIX Dummy-Asoc
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 0

#define VERSION STR(VERSION_PREFIX-MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION)

#define DEVICE_NAME "dummy_card"

static int dummy_dai_link_init(struct snd_soc_pcm_runtime *rtd)
{
	printk("Andrew: %s %s %d\n", __FILE__, __func__, __LINE__);
	return 0;
}

static struct snd_soc_dai_link dummy_dai_link[] = {
	{
                .name = "Audio Port",
                .stream_name = "Stream",
                .cpu_dai_name = "cpu.dai.i2s",
		.platform_name = "cpu.pcm.i2s",
                .nonatomic = 1,
                //.dynamic = 1, 
                .codec_name = "dummy_dsp",
                .codec_dai_name = "codec.dai.i2s",
                .trigger = {
                        SND_SOC_DPCM_TRIGGER_POST,
                        SND_SOC_DPCM_TRIGGER_POST
                },
		.dpcm_playback = 1,
		.init = dummy_dai_link_init,
        },
};

static struct snd_soc_card dummy_card = {
	.owner = THIS_MODULE,
	.name = "dummy-soc-card",
	.dai_link = dummy_dai_link,
	.num_links = ARRAY_SIZE(dummy_dai_link),
	.controls = NULL,
	.num_controls = 0,
};

static int dummy_card_probe(struct platform_device *pdev)
{
	printk("Andrew: %s %s %d\n", __FILE__, __func__, __LINE__);

	dummy_card.dev = &pdev->dev;

	return devm_snd_soc_register_card(&pdev->dev, &dummy_card);
}

static int dummy_card_remove(struct platform_device *pdev)
{
	printk("Andrew: %s %s %d\n", __FILE__, __func__, __LINE__);
	return 0;
}

static const struct platform_device_id dummy_card_platform_id[] = {
	{
		.name = DEVICE_NAME,
		.driver_data = 0,
	},
	{
		/* end */
	},

};
MODULE_DEVICE_TABLE(platform, dummy_card_platform_id);

static const struct of_device_id dummy_card_of_match[] = {
	{
		.compatible = "artech,dummy_machine",
	},
	{
		.compatible = "artech,dummy_card",
	},
	{
		/* end */
	},
};
MODULE_DEVICE_TABLE(of, dummy_card_of_match);

static struct platform_driver dummy_card_drv = {
	.probe	= dummy_card_probe,
	.remove	= dummy_card_remove,
	.id_table = dummy_card_platform_id,
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = of_match_ptr(dummy_card_of_match),
	}
};

module_platform_driver(dummy_card_drv);

MODULE_ALIAS("dummy-asoc");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Linux is not Unix");
MODULE_AUTHOR("andrew, mrju.email@gmail.com");
