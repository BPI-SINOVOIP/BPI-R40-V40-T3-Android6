#include "tr_rotate.h"
#include "sunxi_tr.h"

#define MODULE_NAME "transform"
#define DEVICE_NAME "/dev/transform"
#define ROTATE_SYNC_TIMEOUT_MS 50

#define ALIGN(x,a)	(((x) + (a) - 1L) & ~((a) - 1L))

static int dev_fd = 0;

static int deg2mode(int deg)
{
	switch (deg) {
	case ROTATE_DEG_90:
		return (int)TR_ROT_90;
	case ROTATE_DEG_270:
		return (int)TR_ROT_270;
	case ROTATE_DEG_180:
		return (int)TR_ROT_180;
	case ROTATE_FLIP_H:
		return (int)TR_HFLIP;
	case ROTATE_FLIP_V:
		return (int)TR_VFLIP;
	case ROTATE_DEG_0:
		return (int)TR_ROT_0;
	default:
		return -1;
	}
}

static int convert_tr_fmt(unsigned int fmt)
{
	switch (fmt) {
	case ROTATE_YUV420_SP:
		return (int)TR_FORMAT_YUV420_SP_UVUV;
	case ROTATE_YUV420_P:
		return (int)TR_FORMAT_YUV420_P;
	case ROTATE_ARGB_8888:
		return (int)TR_FORMAT_ARGB_8888;
	case ROTATE_RGB_888:
		return (int)TR_FORMAT_RGB_888;
	case ROTATE_RGB_565:
		return (int)TR_FORMAT_RGB_565;
	default:
		ALOGD("unsupport fmt(%d) for tr.", fmt);
		return -1;
	}
}

static struct image *fix_dst_image(struct image *im, struct image *im_temp)
{
	if (ROTATE_YUV420_SP == im->pixel_format) {
		const unsigned int fix_align = 16;
		memcpy((void *)im_temp, (void *)im, sizeof(*im_temp));
		im_temp->datas[1].stride = ALIGN(im->datas[0].stride >> 1, fix_align);
		if (im_temp->datas[1].stride * im->datas[0].height
			<= im->datas[1].stride * im->datas[1].height) {
			im_temp->pixel_format = ROTATE_YUV420_P;
			im_temp->channels = 3;
			im_temp->datas[1].bpp = 8;
			im_temp->datas[1].align = fix_align;
			memcpy((void *)&(im_temp->datas[2]), (void *)&(im_temp->datas[1]),
				sizeof(im_temp->datas[2]));
			im_temp->datas[2].phy_addr = im_temp->datas[1].phy_addr
				+ im_temp->datas[1].stride * im_temp->datas[1].height;
			//ALOGD("fix dst image ok");
			return im_temp;
		} else {
			//ALOGD("%s failed !", __func__);
			return NULL;
		}
	} else {
		//ALOGD("no need to fix dst image");
		return im;
	}
}

int tr_init(void)
{
	dev_fd = open(DEVICE_NAME, O_RDWR);
	if (0 <= dev_fd) {
		return 0;
	} else {
		ALOGD("open %s fail, errno(%d).", DEVICE_NAME, errno);
		return -1;
	}
}

int tr_quit(void)
{
	close(dev_fd);
	dev_fd = -1;
	return 0;
}

int tr_rotate_sync(struct image *const src, struct image *dst, int deg)
{
	tr_info info;
	unsigned long arg[6];
	unsigned long chan = 0;
	int ret = 0;
	unsigned long addr = 0;
	unsigned int i;
	const unsigned int timeout = 50;
	const unsigned int sleep_time = 1000; // us
	unsigned int time_count;

	struct image *p_dst = dst;
	struct image dst_cp;

	memset((void *)&info, 0, sizeof(info));

	ret = deg2mode(deg);
	if (-1 != ret) {
		info.mode = ret;
	} else {
		ALOGD("%s: bad mode(%d)", __func__, deg);
		return -1;
	}
	ret = convert_tr_fmt(src->pixel_format);
	if (-1 != ret) {
		info.src_frame.fmt = (unsigned char)ret;
	} else {
		ALOGD("%s: bad pixel format(%d)", __func__, src->pixel_format);
		return -1;
	}

	for (i = 0; i < src->channels; ++i) {
		if ((src->datas[i].stride & 0xF)
			|| (NULL == src->datas[i].phy_addr)) {
			ALOGD("src_%d: phy_addr=%p, stride=%d", i,
				src->datas[i].phy_addr, src->datas[i].stride);
			return -1;
		}
	}
#ifdef ROTATION_ON_64BIT
	addr = (unsigned long)(src->datas[0].phy_addr);
	info.src_frame.laddr[0] = (unsigned int)(addr & 0xFFFFFFFF);
	info.src_frame.haddr[0] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
	addr = (unsigned long)(src->datas[1].phy_addr);
	info.src_frame.laddr[1] = (unsigned int)(addr & 0xFFFFFFFF);
	info.src_frame.haddr[1] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
	addr = (unsigned long)(src->datas[2].phy_addr);
	info.src_frame.laddr[2] = (unsigned int)(addr & 0xFFFFFFFF);
	info.src_frame.haddr[2] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
#else
	addr = (unsigned long)(src->datas[0].phy_addr);
	info.src_frame.laddr[0] = (unsigned int)(addr & 0xFFFFFFFF);
	addr = (unsigned long)(src->datas[1].phy_addr);
	info.src_frame.laddr[1] = (unsigned int)(addr & 0xFFFFFFFF);
	addr = (unsigned long)(src->datas[2].phy_addr);
	info.src_frame.laddr[2] = (unsigned int)(addr & 0xFFFFFFFF);
#endif

	info.src_frame.pitch[0] = (src->datas[0].stride << 3) / src->datas[0].bpp;
	info.src_frame.pitch[1] = (src->datas[1].stride << 3) / src->datas[1].bpp;
	info.src_frame.pitch[2] = (src->datas[2].stride << 3) / src->datas[2].bpp;
	info.src_frame.height[0] = src->datas[0].height;
	info.src_frame.height[1] = src->datas[1].height;
	info.src_frame.height[2] = src->datas[2].height;
	info.src_rect.x = src->datas[0].crop.left;
	info.src_rect.y = src->datas[0].crop.top;
	info.src_rect.w = src->datas[0].crop.right - src->datas[0].crop.left;
	info.src_rect.h = src->datas[0].crop.bottom - src->datas[0].crop.top;


	p_dst = fix_dst_image(dst, &dst_cp);
	if (NULL == p_dst) {
		return -1;
	}
	ret = convert_tr_fmt(p_dst->pixel_format);
	if (-1 != ret) {
		info.dst_frame.fmt = (unsigned char)ret;
	} else {
		ALOGD("%s: bad dst pixel format(%d)", __func__, p_dst->pixel_format);
		return -1;
	}

	for (i = 0; i < p_dst->channels; ++i) {
		if (NULL == p_dst->datas[i].phy_addr) {
			ALOGD("dst null phy_addr[%d]", i);
			return -1;
		}
	}
#ifdef ROTATION_ON_64BIT
	addr = (unsigned long)(p_dst->datas[0].phy_addr);
	info.dst_frame.laddr[0] = (unsigned int)(addr & 0xFFFFFFFF);
	info.dst_frame.haddr[0] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
	addr = (unsigned long)(p_dst->datas[1].phy_addr);
	info.dst_frame.laddr[1] = (unsigned int)(addr & 0xFFFFFFFF);
	info.dst_frame.haddr[1] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
	addr = (unsigned long)(p_dst->datas[2].phy_addr);
	info.dst_frame.laddr[2] = (unsigned int)(addr & 0xFFFFFFFF);
	info.dst_frame.haddr[2] = (unsigned char)((addr >> 32) & 0xFFFFFFFF);
#else
	addr = (unsigned long)(p_dst->datas[0].phy_addr);
	info.dst_frame.laddr[0] = (unsigned int)(addr & 0xFFFFFFFF);
	addr = (unsigned long)(p_dst->datas[1].phy_addr);
	info.dst_frame.laddr[1] = (unsigned int)(addr & 0xFFFFFFFF);
	addr = (unsigned long)(p_dst->datas[2].phy_addr);
	info.dst_frame.laddr[2] = (unsigned int)(addr & 0xFFFFFFFF);
#endif

	info.dst_frame.pitch[0] = (p_dst->datas[0].stride << 3) / p_dst->datas[0].bpp;
	info.dst_frame.pitch[1] = (p_dst->datas[1].stride << 3) / p_dst->datas[1].bpp;
	info.dst_frame.pitch[2] = (p_dst->datas[2].stride << 3) / p_dst->datas[2].bpp;
	info.dst_frame.height[0] = p_dst->datas[0].height;
	info.dst_frame.height[1] = p_dst->datas[1].height;
	info.dst_frame.height[2] = p_dst->datas[2].height;
	info.dst_rect.x = p_dst->datas[0].crop.left;
	info.dst_rect.y = p_dst->datas[0].crop.top;
	info.dst_rect.w = p_dst->datas[0].crop.right - p_dst->datas[0].crop.left;
	info.dst_rect.h = p_dst->datas[0].crop.bottom - p_dst->datas[0].crop.top;

	if (ROTATE_YUV420_P == src->pixel_format) {
		if ((3 != src->channels)
			|| (info.src_frame.laddr[0] & 0xF)
			|| (info.src_frame.laddr[1] & 0xF)
			|| (info.src_frame.laddr[2] & 0xF)
			|| (info.src_frame.pitch[0] & 0xF)
			|| (info.src_frame.pitch[1] & 0xF)
			|| (info.src_frame.pitch[2] & 0xF)) {
			ALOGD("wrong src align for transform.");
			return -1;
		}
	} else if (ROTATE_YUV420_SP == src->pixel_format) {
		if ((2 != src->channels)
			|| (info.src_frame.laddr[0] & 0xF)
			|| (info.src_frame.laddr[1] & 0xF)
			|| (info.src_frame.pitch[0] & 0xF)
			|| (info.src_frame.pitch[1] & 0xF)) {
			ALOGD("wrong src align for transform.");
			return -1;
		}
	}

	if (TR_FORMAT_YUV420_P == info.dst_frame.fmt) {
		if ((info.dst_frame.laddr[0] & 0x1F)
			|| (info.dst_frame.laddr[1] & 0x1F)
			|| (info.dst_frame.laddr[2] & 0x1F)
			|| (info.dst_frame.pitch[0] & 0xF)
			|| (info.dst_frame.pitch[1] & 0xF)
			|| (info.dst_frame.pitch[2] & 0xF)) {
			ALOGD("wrong dst align for transform.");
			return -1;
		}
	}

#if 0
	ALOGD("tr_info: mode=%d", info.mode);
	ALOGD("info.src: fmt[%d], pitch[%d,%d,%d], height[%d,%d,%d], "
		"laddr[%x,%x,%x], haddr[%x,%x,%x], rect[%d,%d,%d,%d]",
		info.src_frame.fmt, info.src_frame.pitch[0], info.src_frame.pitch[1],
		info.src_frame.pitch[2], info.src_frame.height[0],
		info.src_frame.height[1], info.src_frame.height[2],
		info.src_frame.laddr[0], info.src_frame.laddr[1],
		info.src_frame.laddr[2], info.src_frame.haddr[0],
		info.src_frame.haddr[1], info.src_frame.haddr[2],
		info.src_rect.x, info.src_rect.y, info.src_rect.w, info.src_rect.h);
	ALOGD("info.dst: fmt[%d], pitch[%d,%d,%d], height[%d,%d,%d], "
		"laddr[%x,%x,%x], haddr[%x,%x,%x], rect[%d,%d,%d,%d]",
		info.dst_frame.fmt, info.dst_frame.pitch[0], info.dst_frame.pitch[1],
		info.dst_frame.pitch[2], info.dst_frame.height[0],
		info.dst_frame.height[1], info.dst_frame.height[2],
		info.dst_frame.laddr[0], info.dst_frame.laddr[1],
		info.dst_frame.laddr[2], info.dst_frame.haddr[0],
		info.dst_frame.haddr[1], info.dst_frame.haddr[2],
		info.dst_rect.x, info.dst_rect.y, info.dst_rect.w, info.dst_rect.h);
#endif

	/* request transform */
	arg[0] = (unsigned long)&chan;
	if (ioctl(dev_fd, TR_REQUEST, (void *)arg) < 0) {
		ALOGD("TR_REQUEST fail.");
		return -1;
	}

	/* set timeout */
	arg[0] = (unsigned long)&chan;
	arg[1] = ROTATE_SYNC_TIMEOUT_MS; //ms
	if (ioctl(dev_fd, TR_SET_TIMEOUT, (void *)arg) < 0) {
		ALOGD("TR_SET_TIMEOUT fail !");
	}

	/* commit transform job */
	arg[0] = chan;
	arg[1] = (unsigned long)&info;
	if (ioctl(dev_fd, TR_COMMIT, (void *)arg) < 0) {
		ALOGD("TR_COMMIT fail !");
		return -1;
	}

	time_count = 0;
	while ((time_count < timeout) && ioctl(dev_fd, TR_QUERY, (void *)arg)) {
		++time_count;
		usleep(sleep_time);
	}
	if (time_count >= timeout) {
		ALOGE("transform timeout!!! timeout=%d ms", time_count);
		return -1;
	}

	if (p_dst != dst) {
		memcpy((void *)dst, (void *)p_dst, sizeof(*dst));
	}
	//ALOGD("%s ok ! time count %d ms!", __func__, time_count);
	return 0;
}

int tr_rotate_async(struct image *const src, struct image *dst, int deg)
{
	ALOGD("%s: no support yet.", __func__);
	return -1;
}

static int tr_rotation_destroy(struct rotation_module *module)
{
	free(module);
	tr_quit();
	return 0;
}

static struct rotation_module *tr_rotation_creat(void)
{
	struct rotation_module *module = NULL;

	if (tr_init())
		return NULL;

	module = (struct rotation_module *)malloc(sizeof(*module));
	if (NULL == module) {
		ALOGD("malloc for tr_rotation module fail.");
		tr_quit();
		return NULL;
	}

	memset((void *)module, 0, sizeof(*module));
	module->name = MODULE_NAME;
	module->destroy = tr_rotation_destroy;
	module->rotate_sync = tr_rotate_sync;
	module->rotate_async = tr_rotate_async;

	return module;
}

struct boot_strap tr_boot_strap = {
	MODULE_NAME,
	"sunxi tr module",
	tr_rotation_creat,
};
