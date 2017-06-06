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

#define LOG_TAG "audio_utils"
#define LOG_NDEBUG 0

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <cutils/log.h>

#include <tinyalsa/asoundlib.h>
#include "audio_utils.h"

int get_audio_card_number(const char *audio_device_name, int *card)
{
	ALOGV("%s() %s", __func__, audio_device_name);
	int ret = 0;
	int i = 0,nLen = 0;
	FILE *fd = NULL;

	for(i = 0; i < CARD_NUM_MAX; i++)
	{
		char *buf;
		char card_id[sizeof(SND_SYS_SOUND_PATH) + 10];
        sprintf(card_id, SND_SYS_SOUND_PATH, i);
		ALOGV("%s() %s", __func__, card_id);
		fd = fopen(card_id, "r");
		if(!fd){
            ALOGD("%s() get card %d id fail.", __func__,i);
			continue;
		}
		//fseek(fd, 0, SEEK_END);
		//nLen = ftell(fd);
		nLen = 10;
		//rewind(fd);
		buf = (char*)malloc(sizeof(char)*nLen);
		if(!buf){
            ALOGE("%s() alloc memory fail.", __func__);
			fclose(fd);
			return -1;
        }
		nLen = fread(buf, sizeof(char), nLen, fd);
		ALOGD("%s() buf = %s", __func__,buf);
		if(!(ret = strncmp(buf,audio_device_name,sizeof(audio_device_name)))){
			ALOGD("%s() get card id = %d.", __func__,i);
            *card = i;
			fclose(fd);
			free(buf);
			return 0;
        }
		fclose(fd);
		free(buf);
    }
    return -1;
}

/* for future*/
int get_audio_card_dev(const char *audio_device_name,int stream, int *card, int *dev)
{
	return 0;
}