#include "aw_rotation.h"
#include "rotation_sys.h"
#include "stdio.h"

#define SIZE_ARRAY(x) (sizeof(x) / sizeof((x)[0]))
#define MIN_WIDTH_RES 4
#define MIN_HEIGHT_RES 4

static struct boot_strap *s_boot_starp[] = {

#ifdef ROTATION_G2D
	&g2d_boot_strap,
#endif

#ifdef ROTATION_TR
	&tr_boot_strap,
#endif

#ifdef ROTATION_NEON
	&neon_boot_strap,
#endif

	&cpu_boot_strap,
	NULL,
};

static unsigned s_modules_num = 0;
static struct rotation_module *s_modules[SIZE_ARRAY(s_boot_starp)];

/* return: 0(false) if invalid, otherwise !0(true) */
static inline unsigned char check_valid_fmt(unsigned int fmt)
{
	return (RATATE_FORMAT_NUM > fmt);
}

/* return: 0(false) if invalid, otherwise !0(true) */
static inline unsigned char check_valid_resolution(
	unsigned int width, unsigned int height)
{
	return ((MIN_WIDTH_RES <= width) && (MIN_HEIGHT_RES <= height));
}

/* return: 0(false) if invalid, otherwise !0(true) */
static inline unsigned char check_valid_degree(int deg)
{
	return ((ROTATE_DEG_90 == deg)
		|| (ROTATE_DEG_180 == deg)
		|| (ROTATE_DEG_270 == deg));
}

/* return: 0(false) if invalid, otherwise !0(true) */
static inline unsigned char check_valid_channels(unsigned int channels)
{
	return (4 > channels);
}

/* return: 0(false) if invalid, otherwise !0(true) */
static unsigned char check_and_fix_crop(
	struct rect *src_crop, struct rect *dst_crop,
	unsigned int width, unsigned int height, int deg)
{
	if ((0 > src_crop->left)
		|| (width < (unsigned int)(src_crop->left)))
		src_crop->left = 0;
	if ((0 > src_crop->top)
		|| (height < (unsigned int)(src_crop->top)))
		src_crop->top = 0;

	if ((src_crop->left >= src_crop->right)
		|| (width < (unsigned int)(src_crop->right)))
		src_crop->right = width;
	if ((src_crop->top >= src_crop->bottom)
		|| (height < (unsigned int)(src_crop->bottom)))
		src_crop->bottom = height;

	switch (deg) {
	case ROTATE_DEG_90:
		dst_crop->left = height - src_crop->bottom;
		dst_crop->top = src_crop->left;
		dst_crop->right = height - src_crop->top;
		dst_crop->bottom = src_crop->right;
		break;
	case ROTATE_DEG_180:
		dst_crop->left = width - src_crop->right;
		dst_crop->top = height - src_crop->bottom;
		dst_crop->right = width - src_crop->left;
		dst_crop->bottom = height - src_crop->top;
		break;
	case ROTATE_DEG_270:
		dst_crop->left = src_crop->top;
		dst_crop->top = width - src_crop->right;
		dst_crop->right = src_crop->bottom;
		dst_crop->bottom = width - src_crop->left;
		break;
	default:
		return 0;
	}

	return !0;
}

/* return: 0(false) if invalid, otherwise !0(true) */
static unsigned char check_fix_geometry(
	struct image *const src, struct image *const dst, int deg)
{
	unsigned int i;
	for (i = 0; i < src->channels; ++i) {
		if (!check_and_fix_crop(&(src->datas[i].crop), &(dst->datas[i].crop),
			src->datas[i].width, src->datas[i].height, deg))
			return 0;
	}
	return !0;
}

/* return: 0(false) if invalid, otherwise !0(true) */
static inline unsigned char check_valid_para(
	struct image *const src, struct image *const dst, int deg)
{
	return ((NULL != src)
		&& (NULL != dst)
		&& check_valid_fmt(src->pixel_format)
		&& check_valid_fmt(dst->pixel_format)
		&& check_valid_channels(src->channels)
		&& check_valid_channels(dst->channels)
		&& check_valid_resolution(src->width, src->height)
		&& check_valid_resolution(dst->width, dst->height)
		&& check_valid_degree(deg)
		&& check_fix_geometry(src, dst, deg));
}

int rotation_init(void)
{
	unsigned int i = 0;
	struct rotation_module *module = NULL;

	memset((void *)s_modules, 0, sizeof(s_modules));
	s_modules_num = 0;
	for (; s_boot_starp[i]; ++i) {
		ALOGD("boot: %s", s_boot_starp[i]->name);
		if (s_boot_starp[i]->create)
			module = s_boot_starp[i]->create();
		else {
			ALOGD("s_boot_starp[%d]: the create func is null", i);
			continue;
		}
		if (NULL != module) {
			ALOGD("Has get %s.", s_boot_starp[i]->desc);
			s_modules[s_modules_num] = module;
			++s_modules_num;
		}
	}

	ALOGD("s_modules_num=%d", s_modules_num);
	if (s_modules_num > 0)
		return 0;
	else
		return -1;
}

int rotation_quit(void)
{
	unsigned int i = 0;

	for (; s_modules[i]; ++i) {
		ALOGD("%s: %s", __func__, s_modules[i]->name);
		if (s_modules[i]->destroy)
			s_modules[i]->destroy(s_modules[i]);
		else
			ALOGD("the destroy function of modules[%d] is null", i);
	}
	if (i != s_modules_num) {
		ALOGE("check it ! (%d <-> %d)", i, s_modules_num);
	}
	memset((void *)s_modules, 0, sizeof(s_modules));
	s_modules_num = 0;
	return 0;
}

int rotate_image_sync(struct image *const src, struct image *dst, int deg)
{
	int ret = 0;
	unsigned int i = 0;

	if (!check_valid_para(src, dst, deg)) {
		ALOGE("%s: wrong para. deg=%d", __func__, deg);
		ALOGE("src: fmt=%d, width=%d, height=%d, channels=%d",
			src->pixel_format, src->width, src->height, src->channels);
		ALOGE("dst: fmt=%d, width=%d, height=%d, channels=%d",
			dst->pixel_format, dst->width, dst->height, dst->channels);
		return -1;
	}

	for (; s_modules[i]; ++i) {
		//ALOGD("%s: %s", __func__, s_modules[i]->name);
		if (s_modules[i]->rotate_sync) {
			ret = s_modules[i]->rotate_sync(src, dst, deg);
			if (!ret) {
				return 0;
			}
		}
	}

	ALOGE("%s failed !", __func__);
	return -1;
}

int rotate_image_async(struct image *const src, struct image *dst, int deg)
{
	return 0;
}
