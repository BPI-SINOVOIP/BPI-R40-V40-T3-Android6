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
#ifndef __AUDIO_HW_EXTERNAL__
#define __AUDIO_HW_EXTERNAL__
#include "../audio_raw.h"

#define F_LOG ALOGV("%s, line: %d", __FUNCTION__, __LINE__);

#define PRO_AUDIO_OUTPUT_ACTIVE_EXTERNAL     "audio.output.active.external"
#define PRO_AUDIO_INPUT_ACTIVE_EXTERNAL      "audio.input.active.external"

#define USERECORD_44100_SAMPLERATE
#define call_button_voice 0
#define MIXER_CARD 0

/* enable denoise for capture */
//#define ENABLE_DENOISE 1


/* ALSA cards */
#define CARD_CODEC      0
#define CARD_HDMI       1
#define CARD_SPDIF      2
#define CARD_DEFAULT    0

/* ALSA ports */
#define PORT_CODEC      0
#define PORT_HDMI       0
#define PORT_SPDIF      0
#define PORT_MODEM      0

#define SAMPLING_RATE_8K    8000
#define SAMPLING_RATE_11K   11025
#define SAMPLING_RATE_44K   44100
#define SAMPLING_RATE_48K   48000

#define AUDIO_MAP_CNT   16
#define AUDIO_NAME_CODEC    "AUDIO_CODEC"
#define AUDIO_NAME_HDMI     "AUDIO_HDMI"
#define AUDIO_NAME_SPDIF    "AUDIO_SPDIF"
#define AUDIO_NAME_I2S      "AUDIO_I2S"
#define AUDIO_NAME_BT       "AUDIO_BT"

/* constraint imposed by ABE: all period sizes must be multiples of 24 */
#define ABE_BASE_FRAME_COUNT 24
/* number of base blocks in a short period (low latency) */
//#define SHORT_PERIOD_MULTIPLIER 44  /* 22 ms */
#define SHORT_PERIOD_MULTIPLIER 55 /* 40 ms */
/* number of frames per short period (low latency) */
//#define SHORT_PERIOD_SIZE (ABE_BASE_FRAME_COUNT * SHORT_PERIOD_MULTIPLIER)
#define SHORT_PERIOD_SIZE (1360*2)
/* number of short periods in a long period (low power) */
//#define LONG_PERIOD_MULTIPLIER 14  /* 308 ms */
#define LONG_PERIOD_MULTIPLIER 6  /* 240 ms */
/* number of frames per long period (low power) */
#define LONG_PERIOD_SIZE (SHORT_PERIOD_SIZE * LONG_PERIOD_MULTIPLIER)
/* number of pseudo periods for playback */
#define PLAYBACK_PERIOD_COUNT 2
/* number of periods for capture */
#define CAPTURE_PERIOD_COUNT 2
/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000

#define RESAMPLER_BUFFER_FRAMES (SHORT_PERIOD_SIZE * 2)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)

/* android default out or in sampling rate*/
// for playback
#define DEFAULT_OUT_SAMPLING_RATE SAMPLING_RATE_44K
// for capture
#define DEFAULT_IN_SAMPLING_RATE SAMPLING_RATE_44K

/* audio codec default sampling rate*/
#define MM_SAMPLING_RATE SAMPLING_RATE_44K

/*wifi display buffer size*/
#define AF_BUFFER_SIZE 1024 * 80

#define MAX_AUDIO_DEVICES   16

#define MAX_PREPROCESSORS 3 /* maximum one AGC + one NS + one AEC per input stream */


struct pcm_buf_manager
{
    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    bool            BufExist;
    unsigned char   *BufStart;
    int             BufTotalLen;
    unsigned char   *BufReadPtr;
    int             DataLen;
    unsigned char   *BufWritPtr;
    int             BufValideLen;
    int             SampleRate;
    int             Channel;
    struct sunxi_audio_device *dev;
};

typedef struct sunxi_audio_device_manager {
    char        name[32];
    char        card_id[32];
    int         card;
    int         device;
    int         flag_in;            //
    int         flag_in_active;     // 0: not used, 1: used to caputre
    int         flag_out;
    int         flag_out_active;    // 0: not used, 1: used to playback
    bool        flag_exist;         // for hot-plugging
}sunxi_audio_device_manager;

struct mixer_ctls {
    struct mixer_ctl *master_playback_volume;
    struct mixer_ctl *audio_spk_headset_switch;
};

struct sunxi_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config;
    struct pcm *pcm;
    int device;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *buffer;
    size_t frames_in;
    unsigned int requested_rate;
    int standby;
    int source;
    struct echo_reference_itfe *echo_reference;
    bool need_echo_reference;
    effect_handle_t preprocessors[MAX_PREPROCESSORS];
    int num_preprocessors;
    int16_t *proc_buf;
    size_t proc_buf_size;
    size_t proc_frames_in;
    int16_t *ref_buf;
    size_t ref_buf_size;
    size_t ref_frames_in;
    int read_status;
#if defined ENABLE_DENOISE
    void *de_handle;
#endif
    struct sunxi_audio_device *dev;
};

struct sunxi_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config;
    struct pcm_config multi_config[16];
    struct pcm *pcm;

    struct pcm *multi_pcm[16];
    struct resampler_itfe *resampler;
    struct resampler_itfe *multi_resampler[16];
    char *buffer;
    int standby;
    struct echo_reference_itfe *echo_reference;
    struct sunxi_audio_device *dev;
    int write_threshold;
};

struct sunxi_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct mixer *mixer;
    struct mixer_ctls mixer_ctls;
    int mode;
    int out_device;
    int in_device;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
    int in_call;
    float voice_volume;
    struct sunxi_stream_in *active_input;
    struct sunxi_stream_out *active_output;
    bool mic_mute;
    int tty_mode;
    struct echo_reference_itfe *echo_reference;
    bool bluetooth_nrec;
    int wb_amr;
    bool raw_flag;       // flag for raw data
    RAW_MODE_t raw_mode; // flag for raw data
    int fm_mode;
    bool in_record;
    bool bluetooth_voice;
    bool af_capture_flag;
    struct pcm_buf_manager PcmManager;
    // add for audio device management
    struct sunxi_audio_device_manager dev_manager[MAX_AUDIO_DEVICES];
    int usb_audio_cnt;
    char in_devices[128];
    char out_devices[128];
    bool first_set_audio_routing;
    struct audio_route *ar;
    int mixer_card;
};

#endif
