#define LOG_TAG "audio_3d_surround"

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <stdbool.h>
#include <stdlib.h>
#include <system/audio.h>

#include "audio_3d_surround.h"

#define SUR_LIB_PATH "libAwHeadpSurround.so"

#define SPACE_GAIN		(0.50)
#define BASS_GAIN		(0.33)
#define DEFINTION_GAIN	(0.80)

int surround_init(struct audio_3d_surround *sur,
		int samp_rate, int chn, int num_frame)
{
	memset(sur, 0, sizeof(*sur));

	/* open lib */
	sur->lib = dlopen(SUR_LIB_PATH, RTLD_LAZY);
	if (NULL == sur->lib) {
		ALOGW("%s,line:%d, can't open surround lib.", __func__, __LINE__);
		return -1;
	}

	/* get 3d srround function */
	sur->process_init			= dlsym(sur->lib, "process_init");
	sur->process_exit			= dlsym(sur->lib, "process_exit");
	sur->surround_pro_in_out	= dlsym(sur->lib, "surround_pro_in_out");
	sur->set_bass				= dlsym(sur->lib, "set_bass");
	sur->set_defintion			= dlsym(sur->lib, "set_defintion");
	sur->set_space				= dlsym(sur->lib, "set_space");

	/* init 3d suround parameter */
	sur->sur_handle = sur->process_init(sur->sur_handle, samp_rate, chn, num_frame);
	sur->set_bass(sur->sur_handle, BASS_GAIN);
	sur->set_defintion(sur->sur_handle, DEFINTION_GAIN);
	sur->set_space(sur->sur_handle, SPACE_GAIN);

	return 0;
}

bool surround_ready(struct audio_3d_surround sur)
{
	if ( (NULL == sur.lib) || (NULL == sur.sur_handle) )
			return false;
	return true;
}

/*
 * if out device is headset and android settings
 * 3d surround switch opened, use surround.
 */
bool surround_use(int out_device)
{
	int hs_on;
	int use;
	char value[PROPERTY_VALUE_MAX];

	/* get the current switch state. Default value is close. */
	property_get("persist.sys.audio_3d_surround", value, "0");
	use = atoi(value);

	hs_on = out_device & (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
	return (hs_on && use);
}

int surround_process(struct audio_3d_surround sur, short *buf, int frames, int channels)
{
	sur.surround_pro_in_out(sur.sur_handle, buf, buf, frames * channels);

	return 0;
}

void surround_exit(struct audio_3d_surround *sur)
{
	if (sur->sur_handle != NULL) {
		sur->process_exit(sur->sur_handle);
		sur->sur_handle = NULL;
	}

	if (sur->lib != NULL) {
		dlclose(sur->lib);
		sur->lib = NULL;
	}
}

