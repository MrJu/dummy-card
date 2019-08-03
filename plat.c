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
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/time.h>

#define STR(x) _STR(x)
#define _STR(x) #x

#define VERSION_PREFIX Dummy-Asoc
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 0

#define VERSION STR(VERSION_PREFIX-MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION)

#define DEVICE_NAME "dummy_platform"

struct buffer_manipulation_tools {
	struct timer_list timer;
	struct snd_pcm_substream *substream;
	uint32_t Bps; //Bytes per second
	unsigned long per_size; //Period size in bytes
	uint32_t pos; //Hardware pointer position in ring buffer
	unsigned long timer_cal; //time taken for each period
	unsigned stop_timer; 
};

static struct snd_pcm_hardware asoc_uspace_pcm_hw = {
	.info =		(SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_SYNC_START |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP),
	.formats =	(SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE),
	.rates =	SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min =		8000,
	.rate_max =		192000,
	.channels_min =		1,
	.channels_max =		8,
	.buffer_bytes_max =	2 * 1024 * 1024,
	.period_bytes_min =	64,
	.period_bytes_max =	(1024 * 1024),
	.periods_min =		2,
	.periods_max =		32,
	.fifo_size =		0,
};

struct buffer_manipulation_tools *get_bmt(struct snd_pcm_substream *ss)
{
	return ss->runtime->private_data;
}

void start_timer(struct buffer_manipulation_tools *bmt)
{
	unsigned long jif;
	if(!bmt->stop_timer) {
		jif = bmt->timer_cal;
		jif = (jif + bmt->Bps - 1)/bmt->Bps;
		printk("addl jiffies: %lu\n", jif);
		mod_timer(&bmt->timer, jiffies + jif);
	}
}

void stop_timer(struct buffer_manipulation_tools *bmt)
{
	bmt->stop_timer = 1;
	del_timer(&bmt->timer);
	bmt->timer.expires = 0;
}

void update_pos(struct buffer_manipulation_tools *bmt)
{
	bmt->pos += bmt->per_size;
}

void timer_callback(struct timer_list *t)
{

	struct buffer_manipulation_tools *bmt  = from_timer(bmt, t, timer);
	struct snd_pcm_substream *substream = bmt->substream;;
	update_pos(bmt);
	snd_pcm_period_elapsed(substream);
	start_timer(bmt);
}

static int asoc_platform_open(struct snd_pcm_substream *substream)
{
	snd_soc_set_runtime_hwparams(substream, &asoc_uspace_pcm_hw);
	return 0;
}

static int asoc_platform_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static snd_pcm_uframes_t asoc_platform_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t rbuf_pos;
	struct buffer_manipulation_tools *bmt;

	bmt = get_bmt(substream);
	rbuf_pos = bytes_to_frames(substream->runtime, bmt->pos);
	if( rbuf_pos >= substream->runtime->buffer_size)
		rbuf_pos %= substream->runtime->buffer_size;
	
	pr_info("In asoc_platform_pcm_pointer tmp %lu\n", rbuf_pos);
	return rbuf_pos;
}

static const struct snd_pcm_ops platform_ops =
{
	.open = asoc_platform_open,
	.close = asoc_platform_close,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = asoc_platform_pcm_pointer,
	.mmap = snd_pcm_lib_default_mmap,
};

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static void platform_pcm_free(struct snd_pcm *pcm)
{
	return;
}

static int platform_soc_probe(struct snd_soc_component *component)
{
	return 0;
}

static struct snd_soc_component_driver soc_pcm_driver = {
	.name		= "cpu.pcm.i2s",
	.probe		= platform_soc_probe,
	.ops		= &platform_ops,
	.pcm_new	= platform_pcm_new,
	.pcm_free	= platform_pcm_free,
};

/*
 * Call backs specific to dai
 */
static int dai_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	return 0;
}

static void dai_pcm_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	return;
}

static int dai_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct buffer_manipulation_tools *bmt = get_bmt(substream);
	
	printk("runtime->access: %u\n", runtime->access);
	printk("runtime->format: %u\n", runtime->format);
	printk("runtime->frame_bits: %u\n", runtime->frame_bits);
	printk("runtime->rate: %u\n", runtime->rate);
	printk("runtime->channels: %u\n", runtime->channels);
	printk("runtime->period_size: %lu\n", runtime->period_size);
	printk("runtime->period_size: %u\n", frames_to_bytes(runtime, runtime->period_size));
	printk("runtime->periods: %u\n", runtime->periods);
	printk("runtime->buffer_size: %lu\n", runtime->buffer_size);
	printk("runtime->buffer_size: %u\n", frames_to_bytes(runtime, runtime->buffer_size));
	bmt->Bps = ((runtime->frame_bits / 8) * runtime->rate);
	printk("Bps: %u\n",bmt->Bps);
	printk("sample-pcm:buffer_size = %ld,"
			"dma_area = %p, dma_bytes = %zu\n",
			runtime->buffer_size, runtime->dma_area, runtime->dma_bytes);
	bmt->per_size = frames_to_bytes(runtime, runtime->period_size);
	printk("HZ: %u\n", HZ);
	bmt->timer_cal = bmt->per_size * HZ;
	bmt->pos = 0;
	return 0;
}

static int dai_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct buffer_manipulation_tools *bmt;

	bmt = kmalloc(sizeof(struct buffer_manipulation_tools), GFP_KERNEL);
	printk("substream->private_data 0x%p\n", substream->private_data);
	substream->runtime->private_data = bmt;
	if(!bmt) {
		printk("Not enough memory\n");
		return -ENOMEM;
	}

	timer_setup(&bmt->timer, timer_callback, 0);
	add_timer(&bmt->timer);
	bmt->substream = substream;

	return snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(params));
}

static int dai_pcm_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	if(substream->runtime->private_data)
		kfree(substream->runtime->private_data);
	return snd_pcm_lib_free_vmalloc_buffer(substream);
	return 0;
}

static int dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct buffer_manipulation_tools *bmt = get_bmt(substream);
	printk("cmd: %x\n", cmd);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			printk("timer_cal: %02lx\n", bmt->timer_cal);
			bmt->stop_timer = 0;
			start_timer(bmt);
		case SNDRV_PCM_TRIGGER_RESUME:
			pr_info("SNDRV_PCM_TRIGGER_RESUME\n");
			return 0;
		case SNDRV_PCM_TRIGGER_STOP:
			pr_info("SNDRV_PCM_TRIGGER_STOP\n");
			stop_timer(bmt);
			return 0;
		case SNDRV_PCM_TRIGGER_SUSPEND:
			pr_info("SNDRV_PCM_TRIGGER_SUSPEND\n");
			return 0;
	}

	return 0;
}

static struct snd_soc_dai_ops platform_pcm_dai_ops = {
	.startup = dai_pcm_startup,
	.shutdown = dai_pcm_shutdown,
	.prepare = dai_pcm_prepare,
	.hw_params = dai_pcm_hw_params,
	.hw_free = dai_pcm_hw_free,
	.trigger = dai_pcm_trigger,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S8 | \
		SNDRV_PCM_FMTBIT_U8 | \
		SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_U16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_U24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE | \
		SNDRV_PCM_FMTBIT_U32_LE | \
		SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)

static struct snd_soc_dai_driver platform_dai[] = {
	{
		.name = "cpu.dai.i2s",
		.id = 0,
		.ops = &platform_pcm_dai_ops,
		.playback = {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		},
	},
};

static int dummy_platform_probe(struct platform_device *pdev)
{
	return devm_snd_soc_register_component(&pdev->dev, &soc_pcm_driver,
			platform_dai, ARRAY_SIZE(platform_dai));
}

static int dummy_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct platform_device_id dummy_pcm_platform_id[] = {
	{
		.name = DEVICE_NAME,
		.driver_data = 0,
	},
	{
		/* end */
	},

};
MODULE_DEVICE_TABLE(platform, dummy_pcm_platform_id);

static const struct of_device_id dummy_platform_of_match[] = {
	{
		.compatible = "artech,dummy_platform",
	},
	{
		/* end */
	},
};
MODULE_DEVICE_TABLE(of, dummy_platform_of_match);

static struct platform_driver dummy_platform_drv = {
	.probe  = dummy_platform_probe,
	.remove = dummy_platform_remove,
	.id_table = dummy_pcm_platform_id,
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = of_match_ptr(dummy_platform_of_match),
	}
};

static int __init dummy_platform_init(void)
{
	return platform_driver_register(&dummy_platform_drv);
}

static void __exit  dummy_platform_exit(void)
{
	platform_driver_unregister(&dummy_platform_drv);
}

module_init(dummy_platform_init);
module_exit(dummy_platform_exit);

MODULE_ALIAS("dummy-asoc");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Linux is not Unix");
MODULE_AUTHOR("andrew, mrju.email@gmail.com");
