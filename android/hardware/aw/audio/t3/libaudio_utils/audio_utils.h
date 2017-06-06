/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __AUDIO_UTIL_H__
#define __AUDIO_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <tinyalsa/asoundlib.h>

/**
 * ALSA card id
 **/
#define SUNXI_PCM_ID_DEFAULT_CODEC     "audiocodec"
/* HDMI SOUND CARD NAME */
#define SUNXI_PCM_ID_HDMI_AUDIO        "sndhdmi"
/* BUILD CODEC SOUND CARD NAME */
#define SUNXI_PCM_ID_BUILD_AUDIO       "audiocodec"
/* SPDIF SOUND CARD NAME */
#define SUNXI_PCM_ID_SPDIF             "sndspdif"

/* SOUND PATH */
#define SND_SYS_SOUND_PATH              "/sys/class/sound/card%i/id"
/* MAX NUMBER OF SOUND CARD IN SYSTEM */
#define CARD_NUM_MAX                     8

int get_audio_card_number(const char *audio_device_name, int *card);
int get_audio_card_dev(const char *audio_device_name,int stream, int *card, int *dev);

#ifdef __cplusplus
}
#endif

#endif