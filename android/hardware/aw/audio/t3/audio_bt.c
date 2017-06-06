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
#define LOG_TAG "audio_bt"
#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <system/thread_defs.h>

#include <cutils/log.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <fcntl.h>

#include "audio_hw.h"
#include "audio_bt.h"

struct pcm_config pcm_config_bt = {
    .channels = 2,
    .rate = DEFAULT_SAMPLE_RATE,
    .period_size = 512,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

int cdr_btcall_init(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data;
    bt_data = calloc(1, sizeof (struct sunxi_cdr_btdata));
    if (!bt_data) {
        ALOGE("failed to allocate bt_data memory");
        return -ENOMEM;
    }
    adev->bt_data = bt_data;

    // init mutex and condition
    if (pthread_mutex_init(&bt_data->lock, NULL)) {
        ALOGE("failed to create mutex lock (%d): %m", errno);
        return -EINVAL;
    }

    if (pthread_cond_init(&bt_data->cond, NULL)) {
        ALOGE("failed to create cond(%d): %m", errno);
        return -EINVAL;
    }

    if (!bt_data->thread1) { // define: private data: bt_data
        pthread_attr_t attr;
        struct sched_param sched = {0};
        sched.sched_priority = ANDROID_PRIORITY_AUDIO;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
        pthread_attr_setschedparam(&attr, &sched);
        if (pthread_create(&bt_data->thread1, &attr, btcall_thread1, adev)) {
            ALOGE("%s() pthread_create btinput to codec output thread failed!!!", __func__);
            pthread_attr_destroy(&attr);
            return -EINVAL;
        }
        ALOGD("###thread1 done.");
        pthread_attr_destroy(&attr);
    }
    return 0;
}

int cdr_btcall_init2(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data;
    bt_data = adev->bt_data;
    if (!bt_data) {
        ALOGE("failed to allocate bt_data memory");
        return -ENOMEM;
    }

    // init mutex and condition
    if(pthread_mutex_init(&bt_data->lock2, NULL)) {
        ALOGE("failed to create mutex lock2 (%d): %m", errno);
        return -EINVAL;
    }

    if(pthread_cond_init(&bt_data->cond2, NULL)) {
        ALOGE("failed to create cond out_cond (%d): %m", errno);
        return -EINVAL;
    }

if (!bt_data->thread2) { // define: private data: bt_data
        pthread_attr_t attr;
        struct sched_param sched = {0};
        sched.sched_priority = ANDROID_PRIORITY_AUDIO;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
        pthread_attr_setschedparam(&attr, &sched);
        if(pthread_create(&bt_data->thread2, &attr, btcall_thread2, adev)){
            ALOGE("%s() pthread_create codec input to bt output thread failed!!!", __func__);
            pthread_attr_destroy(&attr);
            return -EINVAL;
        }
        ALOGD("###thread2 done.");
        pthread_attr_destroy(&attr);
    }
    return 0;
}

static int open_bt_inpcm(struct sunxi_audio_device *adev)
{
    int card = 1;
    int dev = 0;

    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    struct pcm_config config_in = pcm_config_bt;

    config_in.period_size = 512;
    config_in.period_count = 2;
    config_in.rate = CODEC_CATPURE_SAMPLE_RATE;
    config_in.channels = 1;
    //getCardNumbyName(adev,"AUDIO_BT",&card);
    if (card == -1)
        card = 1;

    if (!bt_data->bt_inpcm) {
        bt_data->bt_inpcm = pcm_open(card, dev, PCM_IN, &config_in);

        if (!pcm_is_ready(bt_data->bt_inpcm)) {
            ALOGE("cannot open pcm_in driver: %s", pcm_get_error(bt_data->bt_inpcm));
            bt_data->bt_inpcm = NULL;
            return -ENOMEM;
        }
        ALOGV("%s() ", __func__);
    }
    return 0;
}

static void close_bt_inpcm(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;

    if (!bt_data)
        return;

    if (bt_data->bt_inpcm) {
       pcm_close(bt_data->bt_inpcm);
       bt_data->bt_inpcm = NULL;
       ALOGW("%s() ", __func__);
    } else {
        ALOGW("%s() bt_data->bt_inpcm == NULL", __func__);
    }
}


static int open_codec_outpcm(struct sunxi_audio_device *adev)
{
    int card = 0;
    int dev = 0;
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    struct pcm_config config_out = pcm_config_bt;

    config_out.period_size = 512;
    config_out.period_count = 4;
    config_out.channels = 1;
    config_out.raw_flag = 1; //WARN: donot modify it

    //getCardNumbyName(adev, "AUDIO_CODEC", &card);
    if (card == -1)
        card = 0;

    if (!bt_data->codec_outpcm) {
        bt_data->codec_outpcm = pcm_open(card, dev, PCM_OUT, &config_out);

        if (!pcm_is_ready(bt_data->codec_outpcm )) {
            ALOGE("cannot open output_pcm driver: %s", pcm_get_error(bt_data->codec_outpcm));
            bt_data->codec_outpcm = NULL;
            return -ENOMEM;
        }
        ALOGW("%s() ", __func__);
    }
    return 0;
}

static void close_codec_outpcm(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;

    if (!bt_data)
        return;

    if (bt_data->codec_outpcm) {
        pcm_close(bt_data->codec_outpcm);
        bt_data->codec_outpcm = NULL;
        ALOGW("%s() ", __func__);
    } else {
        ALOGW("%s() bt_data->codec_outpcm == NULL", __func__);
    }
}

static int open_codec_inpcm(struct sunxi_audio_device *adev)
{
    int card = 0;
    int dev = 0;

    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    struct pcm_config config_in = pcm_config_bt;

    config_in.period_size = 512;
    config_in.period_count = 2;
    config_in.rate = CODEC_CATPURE_SAMPLE_RATE;
    config_in.channels = 1;
    //getCardNumbyName(adev,"AUDIO_BT",&card);
    if (card == -1)
        card = 0;

    if (!bt_data->codec_inpcm) {
        bt_data->codec_inpcm = pcm_open(card, dev, PCM_IN, &config_in);

        if (!pcm_is_ready(bt_data->codec_inpcm)) {
            ALOGE("cannot open pcm_in driver: %s", pcm_get_error(bt_data->codec_inpcm));
            bt_data->codec_inpcm = NULL;
            return -ENOMEM;
        }
        ALOGV("%s() ", __func__);
    }
    return 0;
}

static void close_codec_inpcm(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;

    if (!bt_data)
        return;

    if (bt_data->codec_inpcm) {
       pcm_close(bt_data->codec_inpcm);
       bt_data->codec_inpcm = NULL;
       ALOGW("%s() ", __func__);
    } else {
        ALOGW("%s() bt_data->codec_inpcm == NULL", __func__);
    }
}


static int open_bt_outpcm(struct sunxi_audio_device *adev)
{
    int card = 1;
    int dev = 0;
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    struct pcm_config config_out = pcm_config_bt;

    config_out.period_size = 512;
    config_out.period_count = 4;
    config_out.channels = 1;
    config_out.raw_flag = 1; //WARN: donot modify it

    //getCardNumbyName(adev, "AUDIO_CODEC", &card);
    if (card == -1)
        card = 1;

    if (!bt_data->bt_outpcm) {
        bt_data->bt_outpcm = pcm_open(card, dev, PCM_OUT, &config_out);

        if (!pcm_is_ready(bt_data->bt_outpcm )) {
            ALOGE("cannot open bt_outpcm driver: %s", pcm_get_error(bt_data->bt_outpcm));
            bt_data->bt_outpcm = NULL;
            return -ENOMEM;
        }
        ALOGW("####%s() ", __func__);
    }
    return 0;
}

static void close_bt_outpcm(struct sunxi_audio_device *adev)
{
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;

    if (!bt_data)
        return;

    if (bt_data->bt_outpcm) {
        pcm_close(bt_data->bt_outpcm);
        bt_data->bt_outpcm = NULL;
        ALOGW("####%s() ", __func__);
    } else {
        ALOGW("%s() bt_data->codec_outpcm == NULL", __func__);
    }
}

// for bt call read from bt in pcm, send data to codec output
void* btcall_thread1(void *data)
{
    ALOGV("%s enter", __func__);
    int ret = 0;
    void *buffer = NULL;
    int size = 0;

    struct sunxi_audio_device *adev = (struct sunxi_audio_device *)data;
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    while(1) {
        pthread_mutex_lock(&bt_data->lock);
        if (!bt_data->start_work && !bt_data->exit_work) {
            if(bt_data->bt_inpcm)
                close_bt_inpcm(adev);

            if(bt_data->codec_outpcm)
                close_codec_outpcm(adev);

            if(buffer){
                free(buffer);
                buffer = NULL;
            }

            ALOGV("%s wait in line = %d\n",__func__,__LINE__);

            pthread_cond_wait(&bt_data->cond, &bt_data->lock);
            ALOGV("%s start.",__func__);
        }
        if (bt_data->exit_work) {
            ALOGD("###btcall_thread1 exit");
            pthread_mutex_unlock(&bt_data->lock);
            break;
        }
        pthread_mutex_unlock(&bt_data->lock);

        if (!bt_data->bt_inpcm) {
            ret = open_bt_inpcm(adev);
            if (ret != 0) {
                ALOGE("open bt_inpcm pcm failed.");
                break;
            }
        }

        if (!bt_data->codec_outpcm) {
            ret = open_codec_outpcm(adev);
            if (ret != 0) {
                ALOGE("open output pcm failed.");
                break;
            }
        }

        if (!buffer) {
            size = pcm_get_buffer_size(bt_data->bt_inpcm);
            buffer = malloc(size);
            if(!buffer){
                ALOGE("malloc failed, out of memory.");
                break;
            }
        }

        ret = pcm_read_ex(bt_data->bt_inpcm, buffer, size);
        if (ret)
            ALOGE("pcm_read failed, return %s\n", pcm_get_error(bt_data->bt_inpcm));

        ret = pcm_write(bt_data->codec_outpcm, buffer, size);
        if (ret)
            ALOGE("pcm_write failed, return %s\n", pcm_get_error(bt_data->codec_outpcm));
    }

    if (bt_data->bt_inpcm)
        close_bt_inpcm(adev);

    if (bt_data->codec_outpcm)
        close_codec_outpcm(adev);

    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    bt_data->exit_work = 1;
    bt_data->start_work = 0;
    return NULL;
}


// for bt call read from codec mic ,send data to bt pcm
void* btcall_thread2(void *data)
{
    ALOGV("%s enter", __func__);
    int ret = 0;
    void *buffer = NULL;
    int size = 0;

    struct sunxi_audio_device *adev = (struct sunxi_audio_device *)data;
    struct sunxi_cdr_btdata *bt_data = adev->bt_data;
    while(1) {
        pthread_mutex_lock(&bt_data->lock2);
        if (!bt_data->start_work2 && !bt_data->exit_work2) {
            if (bt_data->codec_inpcm)
                close_codec_inpcm(adev);

            if (bt_data->bt_outpcm)
                close_bt_outpcm(adev);

            if (buffer){
                free(buffer);
                buffer = NULL;
            }
            ALOGV("%s wait in line = %d\n",__func__,__LINE__);

            pthread_cond_wait(&bt_data->cond2, &bt_data->lock2);
            ALOGV("%s start.",__func__);

        }
        if (bt_data->exit_work2) {
            ALOGD("%s exit",__func__);
            pthread_mutex_unlock(&bt_data->lock2);
            break;
        }
        pthread_mutex_unlock(&bt_data->lock2);

        if (!bt_data->codec_inpcm) {
            ret = open_codec_inpcm(adev);
            if(ret != 0){
                ALOGE("open codec_inpcm pcm failed.");
                break;
            }
        }

        if (!bt_data->bt_outpcm) {
            ret = open_bt_outpcm(adev);
            if (ret != 0) {
                ALOGE("open bt output pcm failed.");
                break;
            }
        }

        if (!buffer) {
            size = pcm_get_buffer_size(bt_data->codec_inpcm);
            buffer = malloc(size);
            if(!buffer){
                ALOGE("malloc failed, out of memory.");
                break;
            }
        }

        ret = pcm_read_ex(bt_data->codec_inpcm, buffer, size);
        if (ret)
            ALOGE("pcm_read failed, return %s\n", pcm_get_error(bt_data->codec_inpcm));

        ret = pcm_write(bt_data->bt_outpcm, buffer, size);
        if (ret)
            ALOGE("pcm_write failed, return %s\n", pcm_get_error(bt_data->bt_outpcm));
    }

    if (bt_data->codec_inpcm)
        close_codec_inpcm(adev);

    if (bt_data->bt_outpcm)
        close_bt_outpcm(adev);

    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    bt_data->exit_work2 = 1;
    bt_data->start_work2 = 0;
    return NULL;
}
