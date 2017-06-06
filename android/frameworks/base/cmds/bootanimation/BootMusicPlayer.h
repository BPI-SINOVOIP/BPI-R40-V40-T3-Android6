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

#ifndef _BOOTANIMATION_AUDIOPLAYER_H
#define _BOOTANIMATION_AUDIOPLAYER_H

#include <utils/Thread.h>
#include <utils/FileMap.h>
#include <tinyalsa/asoundlib.h>

#define AUDIO_DEVICES_MAX 16

typedef struct audio_device_manager {
    char        name[32];
    char        card_id[32];
    int         card;
    int         device;
    int         flag_in;            //
    int         flag_in_active;     // 0: not used, 1: used to caputre
    int         flag_out;
    int         flag_out_active;    // 0: not used, 1: used to playback
    bool        flag_exist;         // for hot-plugging
}audio_device_manager;


namespace android {

class BootMusicPlayer : public Thread
{
public:
                BootMusicPlayer();
    virtual     ~BootMusicPlayer();
    int      find_name(char * in, char * out);
    int       init_audio_card(int card);
    bool    init();

    void        playFile(const char* path);

private:
    virtual bool        threadLoop();

private:
    int                 mCard;      // ALSA card to use
    int                 mDevice;    // ALSA device to use
    int                 mPeriodSize;
    int                 mPeriodCount;

    int      mMixercard;

    FileMap*            mCurrentFile;
    const char*      mPath;
    audio_device_manager adevMgr[AUDIO_DEVICES_MAX];
    struct pcm *mPcm[AUDIO_DEVICES_MAX];
};

} // namespace android

#endif // _BOOTANIMATION_AUDIOPLAYER_H
