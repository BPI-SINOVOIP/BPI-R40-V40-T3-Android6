
#ifndef __VIRTUAL_SOURCE_H__
#define __VIRTUAL_SOURCE_H__

#include <linux/types.h>

struct display_info {
	int width;
	int height;
	int format;
	int offset;
	int align;
};

#define VIRTUAL_SOURCE_MAGIC	'V'
#define VS_IOC_GET_BUFFER_SIZE	_IOR(VIRTUAL_SOURCE_MAGIC, 0, unsigned int *)
#define VS_IOC_UPDATE_DISPLAY	_IOW(VIRTUAL_SOURCE_MAGIC, 1, struct display_info *)

#endif
