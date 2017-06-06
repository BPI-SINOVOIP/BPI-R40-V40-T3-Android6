#ifndef __AW_ROTATION_H__
#define __AW_ROTATION_H__

#include <system/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	ROTATE_DEG_0 = 0x0,
	ROTATE_FLIP_H = HAL_TRANSFORM_FLIP_H,
	ROTATE_FLIP_V = HAL_TRANSFORM_FLIP_V,
	ROTATE_DEG_90 = HAL_TRANSFORM_ROT_90,
	ROTATE_DEG_180 = HAL_TRANSFORM_ROT_180,
	ROTATE_DEG_270 = HAL_TRANSFORM_ROT_270,
};

/*
@see definitions in android/system/core/include/graphics.h
enum {
    // flip source image horizontally (around the vertical axis)
    HAL_TRANSFORM_FLIP_H    = 0x01,
    // flip source image vertically (around the horizontal axis)
    HAL_TRANSFORM_FLIP_V    = 0x02,
    // rotate source image 90 degrees clockwise
    HAL_TRANSFORM_ROT_90    = 0x04,
    // rotate source image 180 degrees
    HAL_TRANSFORM_ROT_180   = 0x03,
    // rotate source image 270 degrees clockwise
    HAL_TRANSFORM_ROT_270   = 0x07,
    // don't use. see system/window.h
    HAL_TRANSFORM_RESERVED  = 0x08,
};
*/

enum {
	ROTATE_ARGB_8888 = 0, // 32 BITS
	ROTATE_RGB_888, // 24 BITS
	ROTATE_RGB_565, // 16 BITS
	ROTATE_8BPP_MONO, // 8 BITS

	/* SP: semi-planar, P:planar, I:interleaved */
	ROTATE_YUV420_SP,
	ROTATE_YUV420_P,

	RATATE_FORMAT_NUM,
	ROTATE_INVALID_FMT,
};

struct rect {
	int left;
	int top;
	int right;
	int bottom;
};

struct data_base {
	char *phy_addr; // vir_addr cannot be null if this is null
	char *vir_addr; // only hw-rot and phy_addr cannot be null if this is null
	unsigned int bpp; // bpp of this data block
	unsigned int width;
	unsigned int height;
	unsigned int stride; // in bytes
	unsigned int align; // Reverse
	struct rect crop;
};

struct image {
	struct data_base datas[4];
	unsigned int pixel_format;
	unsigned int channels;
	unsigned int width; // in pixel
	unsigned int height; // in pixel
	int acquire_fence;
	int release_fence;
};


int rotation_init(void);
int rotation_quit(void);
int rotate_image_sync(struct image *const src, struct image *dst, int deg);
int rotate_image_async(struct image *const src, struct image *dst, int deg);

#ifdef __cplusplus
}
#endif

#endif
