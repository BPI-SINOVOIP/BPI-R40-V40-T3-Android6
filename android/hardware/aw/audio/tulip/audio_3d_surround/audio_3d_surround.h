#ifndef __AUDIO_3D_SURROUND_H_
#define __AUDIO_3D_SURROUND_H_

/*
 * the 3d surround struct
 *
 */
struct audio_3d_surround {
	void *lib;

	void *sur_handle;
	void *(*process_init)(void *handle, int samp_rate, int chn, int num_frame);
	void (*surround_pro_in_out)(void *handle, short *buf, short *new_sp, int data_num);
	void (*process_exit)(void *handle);
	void (*set_space)(void *handle, double space_gain);
	void (*set_bass)(void *handle, double sub_gain);
	void (*set_defintion)(void *handle, double defintion_gain);
};

/*
 * interface for user
 *
 */
int surround_init(struct audio_3d_surround *sur, int samp_rate, int chn, int num_frame);
bool surround_ready(struct audio_3d_surround sur);
bool surround_use(int out_device);
int surround_process(struct audio_3d_surround sur, short *buf, int frames, int channels);
void surround_exit(struct audio_3d_surround *sur);

#endif
