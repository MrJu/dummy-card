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
#include <linux/slab.h>
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

#define DUMMY_SND_MIN_PERIODS 2
#define DUMMY_SND_MAX_PERIODS 16
#define DUMMY_SND_MIN_PERIOD_BYTES 256
#define DUMMY_SND_MAX_PERIOD_BYTES 0x8000
#define DUMMY_SND_MAX_BUFFER_BYTES (DUMMY_SND_MAX_PERIOD_BYTES \
						* DUMMY_SND_MAX_PERIODS)


static const struct snd_pcm_hardware dummy_dma_snd_hw = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,

	.formats		= (SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_FLOAT_LE),

	.rates			= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min		= 8000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= DUMMY_SND_MAX_BUFFER_BYTES,
	.period_bytes_min	= DUMMY_SND_MIN_PERIOD_BYTES,
	.period_bytes_max	= DUMMY_SND_MAX_PERIOD_BYTES,
	.periods_min		= DUMMY_SND_MIN_PERIODS,
	.periods_max		= DUMMY_SND_MAX_PERIODS,
	.fifo_size		= 0,
};

struct buffer_manipulation_tools {
	struct timer_list timer;
	struct snd_pcm_substream *substream;
	uint32_t Bps; //Bytes per second
	unsigned long per_size; //Period size in bytes
	uint32_t pos; //Hardware pointer position in ring buffer
	unsigned long timer_cal; //time taken for each period
	unsigned stop_timer; 
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
	snd_soc_set_runtime_hwparams(substream, &dummy_dma_snd_hw);
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

#if 0
struct snd_pcm_ops {
	int (*open)(struct snd_pcm_substream *substream);
	int (*close)(struct snd_pcm_substream *substream);
	int (*ioctl)(struct snd_pcm_substream * substream,
		     unsigned int cmd, void *arg);
	int (*hw_params)(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params);
	int (*hw_free)(struct snd_pcm_substream *substream);
	int (*prepare)(struct snd_pcm_substream *substream);
	int (*trigger)(struct snd_pcm_substream *substream, int cmd);
	snd_pcm_uframes_t (*pointer)(struct snd_pcm_substream *substream);
	int (*get_time_info)(struct snd_pcm_substream *substream,
			struct timespec *system_ts, struct timespec *audio_ts,
			struct snd_pcm_audio_tstamp_config *audio_tstamp_config,
			struct snd_pcm_audio_tstamp_report *audio_tstamp_report);
	int (*fill_silence)(struct snd_pcm_substream *substream, int channel,
			    unsigned long pos, unsigned long bytes);
	int (*copy_user)(struct snd_pcm_substream *substream, int channel,
			 unsigned long pos, void __user *buf,
			 unsigned long bytes);
	int (*copy_kernel)(struct snd_pcm_substream *substream, int channel,
			   unsigned long pos, void *buf, unsigned long bytes);
	struct page *(*page)(struct snd_pcm_substream *substream,
			     unsigned long offset);
	int (*mmap)(struct snd_pcm_substream *substream, struct vm_area_struct *vma);
	int (*ack)(struct snd_pcm_substream *substream);
};
#endif

#if 0
static const struct snd_pcm_ops platform_ops =
{
	.open = asoc_platform_open,
	.close = asoc_platform_close,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = asoc_platform_pcm_pointer,
	.mmap = snd_pcm_lib_default_mmap,
};
#endif

static const struct snd_pcm_ops platform_ops =
	.open			= asoc_platform_open,
	.close			= asoc_platform_close,
	.ioctl			= snd_pcm_lib_ioctl,
	.hw_params		= NULL,
	.hw_free		= NULL,
	.prepare		= NULL,
	.trigger		= NULL,
	.pointer		= asoc_platform_pcm_pointer,
	.get_time_info	= NULL,
	.fill_silence	= NULL,
	.copy_user		= NULL,
	.copy_kernel	= NULL,
	.page			= NULL,
	.mmap			= snd_pcm_lib_default_mmap,
	.ack			= NULL,
};

#if 0

static inline void *dummy_dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp)
{
	*dma_handle = (dma_addr_t)NULL;
	return kzalloc(size, GFP_KERNEL);
}

static inline void dummy_dma_free_coherent(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_handle)
{
	kfree(cpu_addr);
}

static int dummy_dma_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = dummy_dma_snd_hw.buffer_bytes_max;
	int i;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->area = dummy_dma_alloc_coherent(pcm->card->dev, size,
			&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;

	for (i = 0; i < DUMMY_SND_MAX_BUFFER_BYTES; i++) {	
		buf->area[i] = i;
	}

	buf->bytes = size;
	buf->private_data = NULL;

	return 0;
}

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = dummy_dma_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			return ret;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = dummy_dma_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			return ret;
	}

	return 0;
}

static void platform_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;
		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dummy_dma_free_coherent(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
		buf->area = NULL;
	}
}
#endif

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	size_t size = dummy_dma_snd_hw.buffer_bytes_max;

	snd_pcm_lib_preallocate_pages_for_all(rtd->pcm,
			SNDRV_DMA_TYPE_CONTINUOUS,
			snd_dma_continuous_data(GFP_KERNEL), size, size);
	return 0;
}

static void platform_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int platform_soc_probe(struct snd_soc_component *component)
{
	return 0;
}

#if 0
struct snd_soc_component_driver {
	const char *name;

	/* Default control and setup, added after probe() is run */
	const struct snd_kcontrol_new *controls;
	unsigned int num_controls;
	const struct snd_soc_dapm_widget *dapm_widgets;
	unsigned int num_dapm_widgets;
	const struct snd_soc_dapm_route *dapm_routes;
	unsigned int num_dapm_routes;

	int (*probe)(struct snd_soc_component *);
	void (*remove)(struct snd_soc_component *);
	int (*suspend)(struct snd_soc_component *);
	int (*resume)(struct snd_soc_component *);

	unsigned int (*read)(struct snd_soc_component *, unsigned int);
	int (*write)(struct snd_soc_component *, unsigned int, unsigned int);

	/* pcm creation and destruction */
	int (*pcm_new)(struct snd_soc_pcm_runtime *);
	void (*pcm_free)(struct snd_pcm *);

	/* component wide operations */
	int (*set_sysclk)(struct snd_soc_component *component,
			  int clk_id, int source, unsigned int freq, int dir);
	int (*set_pll)(struct snd_soc_component *component, int pll_id,
		       int source, unsigned int freq_in, unsigned int freq_out);
	int (*set_jack)(struct snd_soc_component *component,
			struct snd_soc_jack *jack,  void *data);

	/* DT */
	int (*of_xlate_dai_name)(struct snd_soc_component *component,
				 struct of_phandle_args *args,
				 const char **dai_name);
	int (*of_xlate_dai_id)(struct snd_soc_component *comment,
			       struct device_node *endpoint);
	void (*seq_notifier)(struct snd_soc_component *, enum snd_soc_dapm_type,
		int subseq);
	int (*stream_event)(struct snd_soc_component *, int event);
	int (*set_bias_level)(struct snd_soc_component *component,
			      enum snd_soc_bias_level level);

	const struct snd_pcm_ops *ops;
	const struct snd_compr_ops *compr_ops;

	/* probe ordering - for components with runtime dependencies */
	int probe_order;
	int remove_order;

	/*
	 * signal if the module handling the component should not be removed
	 * if a pcm is open. Setting this would prevent the module
	 * refcount being incremented in probe() but allow it be incremented
	 * when a pcm is opened and decremented when it is closed.
	 */
	unsigned int module_get_upon_open:1;

	/* bits */
	unsigned int idle_bias_on:1;
	unsigned int suspend_bias_off:1;
	unsigned int use_pmdown_time:1; /* care pmdown_time at stop */
	unsigned int endianness:1;
	unsigned int non_legacy_dai_naming:1;

	/* this component uses topology and ignore machine driver FEs */
	const char *ignore_machine;
	const char *topology_name_prefix;
	int (*be_hw_params_fixup)(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params);
	bool use_dai_pcm_id;	/* use the DAI link PCM ID as PCM device number */
	int be_pcm_base;	/* base device ID for all BE PCMs */
};
#endif

#if 0
static struct snd_soc_component_driver soc_pcm_driver = {
	.name		= "cpu.pcm.i2s", // DRV_NAME
	.probe		= platform_soc_probe,
	.pcm_new	= platform_pcm_new,
	.pcm_free	= platform_pcm_free,
	.ops		= &platform_ops,
};
#endif

struct snd_soc_component_driver soc_pcm_driver = {
	.name				= "cpu.pcm.i2s", // DRV_NAME

	/* Default control and setup, added after probe() is run */
	.controls			= NULL,
	.num_controls		= 0,
	.dapm_widgets		= NULL,
	.num_dapm_widgets	= 0,
	.dapm_routes		= NULL,
	.num_dapm_routes	= 0,

	.probe				= platform_soc_probe,
	.remove				= NULL,
	.suspend			= NULL,
	.resume				= NULL,

	.read				= NULL,
	.write				= NULL,

	/* pcm creation and destruction */
	.pcm_new			= platform_pcm_new,
	.pcm_free			= platform_pcm_free,

	/* component wide operations */
	.set_sysclk			= NULL,
	.set_pll			= NULL,
	.set_jack			= NULL,

	/* DT */
	.of_xlate_dai_name	= NULL,
	.of_xlate_dai_id	= NULL,
	.seq_notifier		= NULL,
	.stream_event		= NULL,
	.set_bias_level		= NULL,

	.ops				= &platform_ops,
	.compr_ops			= NULL,

	/* probe ordering - for components with runtime dependencies */
	.probe_order		= 0,
	.remove_order		= 0,

	/*
	 * signal if the module handling the component should not be removed
	 * if a pcm is open. Setting this would prevent the module
	 * refcount being incremented in probe() but allow it be incremented
	 * when a pcm is opened and decremented when it is closed.
	 */
	.module_get_upon_open	= 0,

	/* bits */
	.idle_bias_on		= 0,
	.suspend_bias_off	= 0,
	.use_pmdown_time	= 0, /* care pmdown_time at stop */
	.endianness			= 0,
	.non_legacy_dai_naming	= 0,

	/* this component uses topology and ignore machine driver FEs */
	.ignore_machine	= NULL,
	.topology_name_prefix	= NULL,
	.be_hw_params_fixup	= NULL,
	.use_dai_pcm_id		= 0,	/* use the DAI link PCM ID as PCM device number */
	.be_pcm_base		= 0,	/* base device ID for all BE PCMs */
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
	struct snd_pcm_runtime *runtime = substream->runtime;

	bmt = kmalloc(sizeof(struct buffer_manipulation_tools), GFP_KERNEL);
	printk("substream->private_data 0x%p\n", substream->private_data);
	substream->runtime->private_data = bmt;
	if(!bmt) {
		printk("Not enough memory\n");
		return -ENOMEM;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);

	timer_setup(&bmt->timer, timer_callback, 0);
	add_timer(&bmt->timer);
	bmt->substream = substream;

	// return snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(params));

	return 0;
}

static int dai_pcm_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	if(substream->runtime->private_data)
		kfree(substream->runtime->private_data);
	// return snd_pcm_lib_free_vmalloc_buffer(substream);
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

#if 0
struct snd_soc_dai_ops {
	/*
	 * DAI clocking configuration, all optional.
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	int (*set_sysclk)(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir);
	int (*set_pll)(struct snd_soc_dai *dai, int pll_id, int source,
		unsigned int freq_in, unsigned int freq_out);
	int (*set_clkdiv)(struct snd_soc_dai *dai, int div_id, int div);
	int (*set_bclk_ratio)(struct snd_soc_dai *dai, unsigned int ratio);

	/*
	 * DAI format configuration
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	int (*set_fmt)(struct snd_soc_dai *dai, unsigned int fmt);
	int (*xlate_tdm_slot_mask)(unsigned int slots,
		unsigned int *tx_mask, unsigned int *rx_mask);
	int (*set_tdm_slot)(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width);
	int (*set_channel_map)(struct snd_soc_dai *dai,
		unsigned int tx_num, unsigned int *tx_slot,
		unsigned int rx_num, unsigned int *rx_slot);
	int (*get_channel_map)(struct snd_soc_dai *dai,
			unsigned int *tx_num, unsigned int *tx_slot,
			unsigned int *rx_num, unsigned int *rx_slot);
	int (*set_tristate)(struct snd_soc_dai *dai, int tristate);

	int (*set_sdw_stream)(struct snd_soc_dai *dai,
			void *stream, int direction);
	/*
	 * DAI digital mute - optional.
	 * Called by soc-core to minimise any pops.
	 */
	int (*digital_mute)(struct snd_soc_dai *dai, int mute);
	int (*mute_stream)(struct snd_soc_dai *dai, int mute, int stream);

	/*
	 * ALSA PCM audio operations - all optional.
	 * Called by soc-core during audio PCM operations.
	 */
	int (*startup)(struct snd_pcm_substream *,
		struct snd_soc_dai *);
	void (*shutdown)(struct snd_pcm_substream *,
		struct snd_soc_dai *);
	int (*hw_params)(struct snd_pcm_substream *,
		struct snd_pcm_hw_params *, struct snd_soc_dai *);
	int (*hw_free)(struct snd_pcm_substream *,
		struct snd_soc_dai *);
	int (*prepare)(struct snd_pcm_substream *,
		struct snd_soc_dai *);
	/*
	 * NOTE: Commands passed to the trigger function are not necessarily
	 * compatible with the current state of the dai. For example this
	 * sequence of commands is possible: START STOP STOP.
	 * So do not unconditionally use refcounting functions in the trigger
	 * function, e.g. clk_enable/disable.
	 */
	int (*trigger)(struct snd_pcm_substream *, int,
		struct snd_soc_dai *);
	int (*bespoke_trigger)(struct snd_pcm_substream *, int,
		struct snd_soc_dai *);
	/*
	 * For hardware based FIFO caused delay reporting.
	 * Optional.
	 */
	snd_pcm_sframes_t (*delay)(struct snd_pcm_substream *,
		struct snd_soc_dai *);
};
#endif

#if 0
static struct snd_soc_dai_ops platform_pcm_dai_ops = {
	.startup = dai_pcm_startup,
	.shutdown = dai_pcm_shutdown,
	.prepare = dai_pcm_prepare,
	.hw_params = dai_pcm_hw_params,
	.hw_free = dai_pcm_hw_free,
	.trigger = dai_pcm_trigger,
};
#endif

static struct snd_soc_dai_ops platform_pcm_dai_ops = {
	/*
	 * DAI clocking configuration, all optional.
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	.set_sysclk				= NULL,
	.set_pll				= NULL,
	.set_clkdiv				= NULL,
	.set_bclk_ratio			= NULL,

	/*
	 * DAI format configuration
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	.set_fmt				= NULL,
	.xlate_tdm_slot_mask	= NULL,
	.set_tdm_slot			= NULL,
	.set_channel_map		= NULL,
	.get_channel_map		= NULL,
	.set_tristate			= NULL,

	.set_sdw_stream			= NULL,
	/*
	 * DAI digital mute - optional.
	 * Called by soc-core to minimise any pops.
	 */
	.digital_mute			= NULL,
	.mute_stream			= NULL,

	/*
	 * ALSA PCM audio operations - all optional.
	 * Called by soc-core during audio PCM operations.
	 */

	.startup				= dai_pcm_startup,
	.shutdown				= dai_pcm_shutdown,
	.hw_params				= dai_pcm_hw_params,
	.hw_free				= dai_pcm_hw_free,
	.prepare				= dai_pcm_prepare,
	/*
	 * NOTE: Commands passed to the trigger function are not necessarily
	 * compatible with the current state of the dai. For example this
	 * sequence of commands is possible: START STOP STOP.
	 * So do not unconditionally use refcounting functions in the trigger
	 * function, e.g. clk_enable/disable.
	 */
	.trigger				= dai_pcm_trigger,
	.bespoke_trigger		= NULL,
	/*
	 * For hardware based FIFO caused delay reporting.
	 * Optional.
	 */
	.delay					= NULL,
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

#if 0
/*
 * Digital Audio Interface Driver.
 *
 * Describes the Digital Audio Interface in terms of its ALSA, DAI and AC97
 * operations and capabilities. Codec and platform drivers will register this
 * structure for every DAI they have.
 *
 * This structure covers the clocking, formating and ALSA operations for each
 * interface.
 */
struct snd_soc_dai_driver {
	/* DAI description */
	const char *name;
	unsigned int id;
	unsigned int base;
	struct snd_soc_dobj dobj;

	/* DAI driver callbacks */
	int (*probe)(struct snd_soc_dai *dai);
	int (*remove)(struct snd_soc_dai *dai);
	int (*suspend)(struct snd_soc_dai *dai);
	int (*resume)(struct snd_soc_dai *dai);
	/* compress dai */
	int (*compress_new)(struct snd_soc_pcm_runtime *rtd, int num);
	/* Optional Callback used at pcm creation*/
	int (*pcm_new)(struct snd_soc_pcm_runtime *rtd,
		       struct snd_soc_dai *dai);
	/* DAI is also used for the control bus */
	bool bus_control;

	/* ops */
	const struct snd_soc_dai_ops *ops;
	const struct snd_soc_cdai_ops *cops;

	/* DAI capabilities */
	struct snd_soc_pcm_stream capture;
	struct snd_soc_pcm_stream playback;
	unsigned int symmetric_rates:1;
	unsigned int symmetric_channels:1;
	unsigned int symmetric_samplebits:1;

	/* probe ordering - for components with runtime dependencies */
	int probe_order;
	int remove_order;
};

/* SoC PCM stream information */
struct snd_soc_pcm_stream {
	const char *stream_name;
	u64 formats;			/* SNDRV_PCM_FMTBIT_* */
	unsigned int rates;		/* SNDRV_PCM_RATE_* */
	unsigned int rate_min;		/* min rate */
	unsigned int rate_max;		/* max rate */
	unsigned int channels_min;	/* min channels */
	unsigned int channels_max;	/* max channels */
	unsigned int sig_bits;		/* number of bits of content */
};

/* generic dynamic object - all dynamic objects belong to this struct */
struct snd_soc_dobj {
	enum snd_soc_dobj_type type;
	unsigned int index;	/* objects can belong in different groups */
	struct list_head list;
	struct snd_soc_tplg_ops *ops;
	union {
		struct snd_soc_dobj_control control;
		struct snd_soc_dobj_widget widget;
	};
	void *private; /* core does not touch this */
};

#endif

#if 0
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
#endif

static struct snd_soc_dobj dummy_dobj;

/*
 * Digital Audio Interface Driver.
 *
 * Describes the Digital Audio Interface in terms of its ALSA, DAI and AC97
 * operations and capabilities. Codec and platform drivers will register this
 * structure for every DAI they have.
 *
 * This structure covers the clocking, formating and ALSA operations for each
 * interface.
 */
static struct snd_soc_dai_driver platform_dai[] = {
	{
		/* DAI description */
		.name					= "cpu.dai.i2s",
		.id						= 0,
		.base					= 0,
		.snd_soc_dobj			= dummy_dobj,

		/* DAI driver callbacks */
		.probe					= NULL,
		.remove					= NULL,
		.suspend				= NULL,
		.resume					= NULL,
		/* compress dai */
		.compress_new			= NULL,
		/* Optional Callback used at pcm creation*/
		.pcm_new				= NULL,
		/* DAI is also used for the control bus */
		.bus_control			= 0,

		/* ops */
		.ops					= &platform_pcm_dai_ops,
		.cops					= NULL,

		/* DAI capabilities */
		.capture	= {
			.stream_name		= "Capture",
			.formats			= STUB_FORMATS,
			.rates				= STUB_RATES,
			.rate_min			= 0,		/* min rate */
			.rate_max			= 0,		/* max rate */
			.channels_min		= 1,
			.channels_max		= 384,
			.sig_bitsi			= 0,		/* number of bits of content */
		},
		.playback	= {
			.stream_name		= "Playback",
			.formats			= STUB_FORMATS,
			.rates				= STUB_RATES,
			.rate_min			= 0,		/* min rate */
			.rate_max			= 0,		/* max rate */
			.channels_min		= 1,
			.channels_max		= 384,
			.sig_bitsi			= 0,		/* number of bits of content */
		},

		.symmetric_rates		= 0,
		.symmetric_channels		= 0,
		.symmetric_samplebits	= 0,

		/* probe ordering - for components with runtime dependencies */
		.probe_order			= 0,
		.remove_order			= 0,
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
