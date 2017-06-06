/*
 * Copyright (C) 2014 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "BootMusicPlayer"

#include "BootMusicPlayer.h"

#include <androidfw/ZipFileRO.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>


#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define AUDIO_NAME_CODEC    "AUDIO_CODEC"
#define AUDIO_NAME_HDMI     "AUDIO_HDMI"
#define AUDIO_NAME_SPDIF    "AUDIO_SPDIF"
#define AUDIO_NAME_I2S      "AUDIO_I2S"
#define AUDIO_NAME_BT       "AUDIO_BT"



struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

typedef enum AUDIO_DEVICE_MANAGEMENT
{
    AUDIO_IN        = 0x01,
    AUDIO_OUT       = 0x02,
}AUDIO_DEVICE_MANAGEMENT;


typedef struct name_map_t {
    char name_linux[32];
    char name_android[32];
}name_map;


static name_map audio_name[AUDIO_DEVICES_MAX] = {
    {"snddaudio0",       AUDIO_NAME_I2S},//daudio
    {"sndac100",        AUDIO_NAME_CODEC},//ac100 codec
    {"sndac200",        AUDIO_NAME_CODEC},//ac200 codec
    {"audiocodec",      AUDIO_NAME_CODEC},//inside codec
    {"sndhdmi",         AUDIO_NAME_HDMI},
    {"sndhdmiraw",      AUDIO_NAME_HDMI},
    {"sndspdif",        AUDIO_NAME_SPDIF},
};


namespace android {

BootMusicPlayer::BootMusicPlayer()
    :   mCard(-1),
        mDevice(-1),
        mPeriodSize(0),
        mPeriodCount(0),
        mCurrentFile(NULL)
{
}

BootMusicPlayer::~BootMusicPlayer() {
}

int BootMusicPlayer::find_name(char * in, char * out)
 {
	 int index = 0;
 
	 if (in == 0 || out == 0) {
		 ALOGE("error params");
		 return -1;
	 }
 
	 for (; index < AUDIO_DEVICES_MAX; index++) {
		 if (strlen(audio_name[index].name_linux) == 0) {
 
			 //sprintf(out, "AUDIO_USB%d", adev->usb_audio_cnt++);
			 sprintf(out, "AUDIO_USB_%s", in);
			 strcpy(audio_name[index].name_linux, in);
			 strcpy(audio_name[index].name_android, out);
			 ALOGD("linux name = %s, android name = %s",
				 audio_name[index].name_linux,
				 audio_name[index].name_android);
			 return 0;
		 }

		 ALOGD("in = %s", in);
 
		 if (!strcmp(in, audio_name[index].name_linux)) {
			 strcpy(out, audio_name[index].name_android);
			 ALOGD("linux name = %s, android name = %s",
				 audio_name[index].name_linux,
				 audio_name[index].name_android);
			 return 0;
		 }
	 }
 
	 return 0;
 }


 int BootMusicPlayer::init_audio_card(int card)
{
    FILE *file;
    int ret = -1;
    const char * snd_path = "/sys/class/sound";
    char snd_card[128], snd_node[128];
    char snd_id[32], snd_name[32];

    memset(snd_card, 0, sizeof(snd_card));
    memset(snd_node, 0, sizeof(snd_node));
    memset(snd_id, 0, sizeof(snd_id));
    memset(snd_name, 0, sizeof(snd_name));

    sprintf(snd_card, "%s/card%d", snd_path, card);
    ret = access(snd_card, F_OK);
    if(ret == 0) {
        // id / name
        sprintf(snd_node, "%s/card%d/id", snd_path, card);

        file = fopen(snd_node, "rb");
        if (file) {
            ret = fread(snd_id, sizeof(snd_id), 1, file);
            int len = strlen(snd_id);
            if(len > 0)
            {
                snd_id[len - 1] = 0;
            }
            fclose(file);
        } else {
            ALOGE("open %s failed!!", snd_node);
            return -1;
        }

        strcpy(adevMgr[card].card_id, snd_id);
        find_name(snd_id, snd_name);
        strcpy(adevMgr[card].name, snd_name);

        adevMgr[card].card = card;
        adevMgr[card].device = 0;
        adevMgr[card].flag_exist = true;

        // playback device
        sprintf(snd_node, "%s/card%d/pcmC%dD0p", snd_path, card, card);
        ret = access(snd_node, F_OK);
        if(ret == 0) {
            // there is a playback device
            adevMgr[card].flag_out = AUDIO_OUT;
            adevMgr[card].flag_out_active = 0;
        }
    } else {
        return -1;
    }

    return 0;
}


bool BootMusicPlayer::init()
{
    struct mixer* mixer = NULL;
    int card = 0;
    mMixercard = -1;
    for (card = 0; card < AUDIO_DEVICES_MAX; card++) {
	memset(&adevMgr[card], 0, sizeof(struct audio_device_manager));
        if (init_audio_card(card) == 0) {
        if(!strcmp(adevMgr[card].name, AUDIO_NAME_CODEC))
        {
            ALOGV("use card %d %s mixer control", card, adevMgr[card].name);
            ALOGV("use card %d %s to play boot music", card, adevMgr[card].name);
            mMixercard = card;
            mCard = card;
        }
        }
    }

    if(mMixercard < 0)
    {
        ALOGE("can not find mixer card !!!");
	 return false;
    }
    mixer = mixer_open(mMixercard);
    if (!mixer) {
        ALOGE("could not open mixer for card %d", mMixercard);
        return false;
    }
    
    mDevice = 0;
    mPeriodSize = 1024;
    mPeriodCount = 4;

    mixer_close(mixer);

    if (mCard >= 0 && mDevice >= 0) {
        return true;
    }

    return false;
}

void BootMusicPlayer::playFile(const char *path) {
    // stop any currently playing sound
    requestExitAndWait();

    mPath = path;
    run("bootanim audio", PRIORITY_URGENT_AUDIO);
}

//#define MULTI_PCM_ENABLE
bool BootMusicPlayer::threadLoop()
{
    FILE *file;
    struct pcm_config config;
    char *buffer;
    int size;
    int card = 0;
    int num_read;
    int more_chunks = 1;
    struct riff_wave_header wavHeader;
    struct chunk_header chunkHeader;
    struct chunk_fmt chunkFmt;
    file = fopen(mPath, "rb");
    if (!file) {
        ALOGE("Unable to open file '%s'\n", mPath);
        return false;
    }

    fread(&wavHeader, sizeof(riff_wave_header), 1, file);
    if ((wavHeader.riff_id != ID_RIFF) ||
        (wavHeader.wave_id != ID_WAVE)) {
        ALOGE("Error: '%s' is not a riff/wave file\n", mPath);
        fclose(file);
        return false;
    }

    do {
        fread(&chunkHeader, sizeof(chunk_header), 1, file);

        switch (chunkHeader.id) {
        case ID_FMT:
            fread(&chunkFmt, sizeof(chunk_fmt), 1, file);
            /* If the format header is larger, skip the rest */
            if (chunkHeader.sz > sizeof(chunkFmt))
                fseek(file, chunkHeader.sz - sizeof(chunkFmt), SEEK_CUR);
            break;
        case ID_DATA:
            /* Stop looking for chunks */
            more_chunks = 0;
            break;
        default:
            /* Unknown chunk, skip bytes */
            fseek(file, chunkHeader.sz, SEEK_CUR);
        }
    } while (more_chunks);

    memset(&config, 0, sizeof(config));
    config.channels = chunkFmt.num_channels;
    config.rate = chunkFmt.sample_rate;
    config.period_size = mPeriodSize;
    config.period_count = mPeriodCount;
    if (chunkFmt.bits_per_sample == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (chunkFmt.bits_per_sample == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
    config.raw_flag = 1;

#ifdef MULTI_PCM_ENABLE
    int index = 0;
     for (index = 0; index < AUDIO_DEVICES_MAX; index++) {
        if (adevMgr[index].flag_exist
            && (adevMgr[index].flag_out == AUDIO_OUT)
            && adevMgr[index].flag_out_active) {
            card = index;
            ALOGV("use %s to playback audio", adevMgr[index].name);
            mPcm[card] = NULL;

            mPcm[card] = pcm_open(card, mDevice, PCM_OUT, &config);
            if (!mPcm[card] || !pcm_is_ready(mPcm[card])) {
                ALOGE("Unable to open PCM card %d device %d", card, mDevice);
        	 fclose(file);
                return false;
            }
        }
    }
    size = pcm_frames_to_bytes(mPcm[0], pcm_get_buffer_size(mPcm[0]));
#else
    struct pcm *pcm;
    pcm = pcm_open(mCard, mDevice, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        ALOGE("Unable to open PCM card %d device %d", card, mDevice);
        fclose(file);
        return false;
    }
    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
#endif

    buffer = (char *)malloc(size);
    if (!buffer) {
        ALOGE("Unable to allocate %d bytes", size);
        free(buffer);
	 fclose(file);
	 #ifdef MULTI_PCM_ENABLE
	 for (index = 0; index < AUDIO_DEVICES_MAX; index++) {
	     if(mPcm[index])
	     {
                pcm_close(mPcm[index]);
		 mPcm[index] = NULL;
	     }
	 }
	 #else
        if(pcm)
        {
            pcm_close(pcm);
            pcm = NULL;
        }
	 #endif
        return false;
    }

    ALOGD("Playing boot music");

    do {
        num_read = fread(buffer, 1, size, file);
        if (num_read > 0) {
	    #ifdef MULTI_PCM_ENABLE
	    for (index = 0; index < AUDIO_DEVICES_MAX; index++) {
	        if (adevMgr[index].flag_exist
               && (adevMgr[index].flag_out == AUDIO_OUT)
               /*&& adevMgr[index].flag_out_active*/) {
                   if (pcm_write(mPcm[index], buffer, num_read)) {
                       ALOGE("pcm_write failed (%s)", pcm_get_error(mPcm[index]));
                       break;
                   }
	        }
	    }
	    #else
	    if (pcm_write(pcm, buffer, num_read)) {
               ALOGE("pcm_write failed (%s)", pcm_get_error(pcm));
               break;
            }
	    #endif
        }
    } while (num_read > 0);

    free(buffer);
    fclose(file);
    #ifdef MULTI_PCM_ENABLE
    for (index = 0; index < AUDIO_DEVICES_MAX; index++) {
        if(mPcm[index])
        {
            pcm_close(mPcm[index]);
            mPcm[index] = NULL;
        }
    }
    #else
	if(pcm)
        {
            pcm_close(pcm);
            pcm = NULL;
        }
    #endif
    return false;
}

} // namespace android
