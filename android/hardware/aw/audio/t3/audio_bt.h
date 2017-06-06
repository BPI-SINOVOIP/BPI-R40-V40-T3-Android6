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
#ifndef __AUDIO_BT__
#define __AUDIO_BT__

#define CODEC_CATPURE_SAMPLE_RATE  8000
#define CODEC_PLAYBACK_SAMPLE_RATE  8000

#define BT_CATPURE_SAMPLE_RATE  8000
#define BT_PLAYBACK_SAMPLE_RATE  8000

#define DEFAULT_SAMPLE_RATE 8000

int cdr_btcall_init(struct sunxi_audio_device *adev);
int cdr_btcall_init2(struct sunxi_audio_device *adev);

void* btcall_thread1(void *data);
void* btcall_thread2(void *data);


struct sunxi_cdr_btdata {
    int start_work;
    int exit_work;
    int start_work2;
    int exit_work2;
    struct pcm *bt_inpcm;
    struct pcm *bt_outpcm;
    struct pcm *codec_inpcm;
    struct pcm *codec_outpcm;
    pthread_t thread1;
    pthread_t thread2;
    pthread_cond_t cond;
    pthread_cond_t cond2;
    pthread_mutex_t lock;
    pthread_mutex_t lock2;
};

#endif
