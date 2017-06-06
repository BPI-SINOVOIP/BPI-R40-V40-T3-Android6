#ifndef NoiseReduction_H
#define NoiseReduction_H


/*
	description:
		create and initialise a controller handle
	input:
		   handle: the handle
		     flag: 0/1  0: channel 1 is the main signal; 1: channel 2 is the main signal
		samp_rate: support 8000 & 16000

	return value:
		controller handle
*/
void*  rt_init(void *handle, int flag, int sample_rate);

/*
	description
		decreate the controller handle
	input:
		handle: the handle
	return value:
		none
*/
void rt_end(void *handle);


/*
	description:
		process one frame
	input:
		   handle: the controller handle
		       in: the signal to process (stereo pcm)
			  out: the signal output (mono pcm)
		  datanum: input data length in short (8000: 64*2; 16000: 128*2)
	return value:
		none
*/
void rt_dec(void *handle,short *in,short *out,int datanum);


#endif

