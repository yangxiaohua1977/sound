/*
 *  cht_bsw_es8316.c - ASoc Machine driver for Intel Byt/CHT CR platform
 *
 *  Author: David Yang <yangxiaohua@everest-semi.com>
 *			<info@everest-semi.com> 
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <asm/cpu_device_id.h>
#include <asm/platform_sst_audio.h>
#include <linux/clk.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../../codecs/es8316.h"
#include "../atom/sst-atom-controls.h"
#include "../common/sst-acpi.h"
#include "../common/sst-dsp.h"
struct byt_es8316_private {
	struct clk *mclk;
	struct snd_soc_jack jack;
	struct cht_acpi_card *acpi_card;
	char codec_name[16];
};

#define BYT_CODEC_DAI1	"ES8316 HiFi"

static inline struct snd_soc_dai *byt_get_codec_dai(struct snd_soc_card *card)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (!strncmp(rtd->codec_dai->name, BYT_CODEC_DAI1,
			     strlen(BYT_CODEC_DAI1)))
			return rtd->codec_dai;
	}
	return NULL;
}

static int platform_clock_control(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai;
	struct byt_es8316_private *priv = snd_soc_card_get_drvdata(card);
	int ret;

	codec_dai = byt_get_codec_dai(card);
	if (!codec_dai) {
		dev_err(card->dev,
			"Codec dai not found; Unable to set platform clock\n");
		return -EIO;
	}
	if (SND_SOC_DAPM_EVENT_ON(event)) {
			ret = clk_prepare_enable(priv->mclk);
			if (ret < 0) {
				dev_err(card->dev,
					"could not configure MCLK state");
				return ret;
			}	
	} else {
		ret = clk_prepare_enable(priv->mclk);
	}

	if (ret < 0) {
		dev_err(card->dev, "can't set codec sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_dapm_widget byt_es8316_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			    platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMD),
};



static const struct snd_soc_dapm_route byt_es8316_audio_map[] = {
        {"MIC1", NULL, "Headset Mic"},
        {"MIC2", NULL, "Internal Mic"},
 
        {"Headphone", NULL, "HPOL"},
        {"Headphone", NULL, "HPOR"},
        {"Speaker", NULL, "HPOL"},
        {"Speaker", NULL, "HPOR"},

	{"Playback", NULL, "ssp2 Tx"},
        {"ssp2 Tx", NULL, "codec_out0"},
        {"ssp2 Tx", NULL, "codec_out1"},
        {"codec_in0", NULL, "ssp2 Rx" },
        {"codec_in1", NULL, "ssp2 Rx" },
        {"ssp2 Rx", NULL, "Capture"},

        {"Headphone", NULL, "Platform Clock"},
        {"Headset Mic", NULL, "Platform Clock"},
        {"Internal Mic", NULL, "Platform Clock"},
        {"Speaker", NULL, "Platform Clock"},

};

static const struct snd_kcontrol_new byt_es8316_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
};



static int byt_es8316_aif1_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)

{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 19200000,
				     params_rate(params) * 512,
				     SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec clock %d\n", ret);
		return ret;
	}
	
	return 0;
}



static int byt_es8316_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_card *card = runtime->card;
	struct byt_es8316_private *priv = snd_soc_card_get_drvdata(card);
	printk("!!!!!!!!!!!!!!!!!!!!!!!Enter Into %s \n", __func__);
	card->dapm.idle_bias_off = true;
	ret = clk_prepare_enable(priv->mclk);
	if(!ret)
	{
		printk("clk_prepare_enable, ret = 0\n");
		clk_disable_unprepare(priv->mclk);
	}
	ret = clk_set_rate(priv->mclk, 19200000);
	if (ret)
		printk("%s, unable to set MCLK rate\n", __func__);
	printk("Exit %s\n", __func__);
	return ret;
}

static const struct snd_soc_pcm_stream byt_es8316_dai_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static int byt_es8316_codec_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	int ret;
	printk("Enter into %s\n", __func__);
	/* The DSP will covert the FE rate to 48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP2 to 24-bit */
	params_set_format(params, SNDRV_PCM_FORMAT_S24_LE);
	/*
	 * Default mode for SSP configuration is TDM 4 slot, override config
	 * with explicit setting to I2S 2ch 24-bit. The word length is set with
	 * dai_set_tdm_slot() since there is no other API exposed
	 */
	ret = snd_soc_dai_set_fmt(rtd->cpu_dai,
				SND_SOC_DAIFMT_I2S     |
				SND_SOC_DAIFMT_NB_IF   |
				SND_SOC_DAIFMT_CBS_CFS
		);
	if (ret < 0) {
		printk("can't set format to I2S, err %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x3, 0x3, 2, 24);
	if (ret < 0) {
		printk("can't set I2S config, err %d\n", ret);
		return ret;
	}
	printk("Exit %s\n", __func__);
	return 0;
}

static int byt_es8316_aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_RATE, 48000);
}

static const struct snd_soc_ops byt_es8316_aif1_ops = {
	.startup = byt_es8316_aif1_startup,
};

static const struct snd_soc_ops byt_es8316_be_ssp2_ops = {
	.hw_params = byt_es8316_aif1_hw_params,
};

static struct snd_soc_dai_link byt_es8316_dais[] = {
	[MERR_DPCM_AUDIO] = {
		.name = "Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "media-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
		.ignore_suspend = 1,
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &byt_es8316_aif1_ops,
	},

	[MERR_DPCM_DEEP_BUFFER] = {
		.name = "Deep-Buffer Audio Port",
		.stream_name = "Deep-Buffer Audio",
		.cpu_dai_name = "deepbuffer-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
		.ignore_suspend = 1,
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.ops = &byt_es8316_aif1_ops,
	},

	[MERR_DPCM_COMPR] = {
		.name = "Compressed Port",
		.stream_name = "Compress",
		.cpu_dai_name = "compress-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
	},

		/* back ends */
	{
		.name = "SSP2-Codec",
		.be_id = 1,
		.cpu_dai_name = "ssp2-port", /* overwritten for ssp0 routing */
		.platform_name = "sst-mfld-platform",
		.no_pcm = 1,
		.codec_dai_name = "ES8316 HiFi", /* changed w/ quirk */
		.codec_name = "es8316.1-0011", /* overwritten with HID */
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS,
		.be_hw_params_fixup = byt_es8316_codec_fixup,
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = byt_es8316_init,
		.ops = &byt_es8316_be_ssp2_ops,
	},
};


/* SoC card */
static struct snd_soc_card byt_es8316_card = {
	.name = "cht-es8316",
	.owner = THIS_MODULE,
	.dai_link = byt_es8316_dais,
	.num_links = ARRAY_SIZE(byt_es8316_dais),
	.dapm_widgets = byt_es8316_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_es8316_widgets),
	.dapm_routes = byt_es8316_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_es8316_audio_map),
	.controls = byt_es8316_controls,
	.num_controls = ARRAY_SIZE(byt_es8316_controls),
};

static char byt_es8316_codec_name[16]; /* i2c-<HID>:00 with HID being 8 chars */

static int snd_byt_es8316_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct byt_es8316_private *priv;
	struct sst_acpi_mach *mach;
	const char *i2c_name = NULL;
	int i;
	int dai_index;
	printk("!!!!!!!!!!!!!!!!!!!!!!!Enter Into %s \n", __func__);
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_ATOMIC);
	if (!priv)
		return -ENOMEM;
	/* register the soc card */
	byt_es8316_card.dev = &pdev->dev;
	mach = byt_es8316_card.dev->platform_data;

	/* fix index of codec dai */
	dai_index = MERR_DPCM_COMPR + 1;
	printk("%s, dai_index = %d \n", __func__, dai_index);
	printk("%s, start to fix index of dai \n", __func__);
	for (i = 0; i < ARRAY_SIZE(byt_es8316_dais); i++) {
		if (!strcmp(byt_es8316_dais[i].codec_name, "i2c-ESSX8316:00")) {
			dai_index = i;
			printk("%s, dai_index = %d \n", __func__, dai_index);
			break;
		}
	}
	/* fixup codec name based on HID */
	i2c_name = sst_acpi_find_name_from_hid(mach->id);
	if (i2c_name != NULL) {
		snprintf(byt_es8316_codec_name, sizeof(byt_es8316_codec_name),
			"%s%s", "i2c-", i2c_name);
		printk("%s, i2c_name = %s\n", __func__, i2c_name);
	}

	snd_soc_card_set_drvdata(&byt_es8316_card, priv);
	ret_val = snd_soc_register_card(&byt_es8316_card);
	if (ret_val) {
		printk("snd_soc_register_card failed %d\n", ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, &byt_es8316_card);

        priv->mclk = devm_clk_get(&pdev->dev, "pmc_plt_clk_3");
	printk("Exit %s\n", __func__);
	return ret_val;
}

static const struct acpi_device_id byt_acpi_match[] = {
	{ "808622A8", 0 },
	{ "ESSX8316", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, byt_acpi_match);

static struct platform_driver snd_byt_es8316_mc_driver = {
	.driver = {
		.name = "cht-bsw-es8316",
		.acpi_match_table = ACPI_PTR(byt_acpi_match),
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_byt_es8316_mc_probe,
};

module_platform_driver(snd_byt_es8316_mc_driver);
MODULE_DESCRIPTION("ASoC Intel(R) Cherrytrail/Baytrail-CR Machine driver");
MODULE_AUTHOR("DavidYang <yangxiaohua@everest-semi.com / info@everest-semi.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cht-bsw-es8316");
