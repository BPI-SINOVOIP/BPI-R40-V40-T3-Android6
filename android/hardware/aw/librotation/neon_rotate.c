#include "neon_rotate.h"


#define MODULE_NAME "neon_rot"

typedef  void (*ROT_FUNC)(char *src_addr, const unsigned int src_widthstep,
		const unsigned int src_width, const unsigned int src_height,
		char *dst_addr, const unsigned int dst_widthstep);

extern void neon_rot_90_u32_2x2(int *src_addr, const unsigned int src_widthstep,
	int *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_180_u32_4x1(int *src_addr, const unsigned int src_widthstep,
	int *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_270_u32_2x2(int *src_addr, const unsigned int src_widthstep,
	int *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_90_u16_4x2(short *src_addr, const unsigned int src_widthstep,
	short *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_180_u16_4x1(short *src_addr, const unsigned int src_widthstep,
	short *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_270_u16_4x2(short *src_addr, const unsigned int src_widthstep,
	short *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_90_u8_8x2(char *src_addr, const unsigned int src_widthstep,
	char *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_180_u8_8x1(char *src_addr, const unsigned int src_widthstep,
	char *dst_addr, const unsigned int dst_widthstep);
extern void neon_rot_270_u8_8x2(char *src_addr, const unsigned int src_widthstep,
	char *dst_addr, const unsigned int dst_widthstep);

static void rot_90_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_widthstep_x2 = src_widthstep << 1;
	const unsigned int dst_widthstep_x2 = dst_widthstep << 1;
	int *dst_addr_end = dst_addr - 2;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr > dst_addr_end; dst_addr -= 2) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 2) {
			neon_rot_90_u32_2x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst = (int *)((char *)p_dst + dst_widthstep_x2);
		}
		src_addr = (int *)((char *)src_addr + src_widthstep_x2);
	}
}

static void rot_180_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	int *dst_addr_end = dst_addr + src_width - 4;
	dst_addr_end  = (int *)((char *)dst_addr_end - dst_widthstep);
	dst_addr = (int *)((char *)dst_addr_end + dst_widthstep * src_height);
	for (; dst_addr > dst_addr_end;) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 4) {
			neon_rot_180_u32_4x1(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst -= 4;
		}
		src_addr = (int *)((char *)src_addr + src_widthstep);
		dst_addr = (int *)((char *)dst_addr - dst_widthstep);
	}
}

static void rot_270_u32(
	int *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	int *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_widthstep_x2 = src_widthstep << 1;
	const unsigned int dst_widthstep_x2 = dst_widthstep << 1;
	int *dst_addr_end;
	dst_addr = (int *)((char *)dst_addr + dst_widthstep * (src_width - 2));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr < dst_addr_end; dst_addr += 2) {
		int *p_src = src_addr;
		int *p_src_end = src_addr + src_width;
		int *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 2) {
			neon_rot_270_u32_2x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst = (int *)((char *)p_dst - dst_widthstep_x2);
		}
		src_addr = (int *)((char *)src_addr + src_widthstep_x2);
	}
}

static void rot_90_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_widthstep_x2 = src_widthstep << 1;
	const unsigned int dst_widthstep_x4 = dst_widthstep << 2;
	short *dst_addr_end = dst_addr - 2;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr > dst_addr_end; dst_addr -= 2) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 4) {
			neon_rot_90_u16_4x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst = (short *)((char *)p_dst + dst_widthstep_x4);
		}
		src_addr = (short *)((char *)src_addr + src_widthstep_x2);
	}
}

static void rot_180_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	short *dst_addr_end = dst_addr + src_width - 4;
	dst_addr_end = (short *)((char *)dst_addr_end - dst_widthstep);
	dst_addr  = (short *)((char *)dst_addr_end + dst_widthstep * src_height);
	for (; dst_addr > dst_addr_end;) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 4) {
			neon_rot_180_u16_4x1(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst -= 4;
		}
		src_addr = (short *)((char *)src_addr + src_widthstep);
		dst_addr = (short *)((char *)dst_addr - dst_widthstep);
	}
}

static void rot_270_u16(
	short *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	short *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_widthstep_x2 = src_widthstep << 1;
	const unsigned int dst_widthstep_x4 = dst_widthstep << 2;
	short *dst_addr_end;
	dst_addr = (short *)((char *)dst_addr + dst_widthstep * (src_width - 4));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr < dst_addr_end; dst_addr += 2) {
		short *p_src = src_addr;
		short *p_src_end = src_addr + src_width;
		short *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 4) {
			neon_rot_270_u16_4x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst = (short *)((char *)p_dst - dst_widthstep_x4);
		}
		src_addr = (short *)((char *)src_addr + src_widthstep_x2);
	}
}

static void rot_90_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	const unsigned int src_widthstep_x2 = src_widthstep << 1;
	const unsigned int dst_widthstep_x8 = dst_widthstep << 3;
	char *dst_addr_end = dst_addr - 2;
	dst_addr = dst_addr_end + src_height;
	for (; dst_addr > dst_addr_end; dst_addr -= 2) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 8) {
			neon_rot_90_u8_8x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst += dst_widthstep_x8;
		}
		src_addr += src_widthstep_x2;
	}
}

static void rot_180_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	char *dst_addr_end = dst_addr - dst_widthstep + src_width - 8;
	dst_addr = dst_addr_end + dst_widthstep * src_height;
	for (; dst_addr > dst_addr_end; dst_addr -= dst_widthstep) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 8) {
			neon_rot_180_u8_8x1(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst -= 8;
		}
		src_addr += src_widthstep;
	}
}

static void rot_270_u8(
	char *src_addr, const unsigned int src_widthstep,
	const unsigned int src_width, const unsigned int src_height,
	char *dst_addr, const unsigned int dst_widthstep)
{
	char *dst_addr_end;
	dst_addr += (dst_widthstep * (src_width - 8));
	dst_addr_end = dst_addr + src_height;
	for (; dst_addr < dst_addr_end; dst_addr += 2) {
		char *p_src = src_addr;
		char *p_src_end = src_addr + src_width;
		char *p_dst = dst_addr;
		for (; p_src < p_src_end; p_src += 8) {
			neon_rot_270_u8_8x2(p_src, src_widthstep, p_dst, dst_widthstep);
			p_dst -= dst_widthstep * 8;
		}
		src_addr += src_widthstep * 2;
	}
}

// [FORMAT][CHANNEL][DEGREE]
static ROT_FUNC g_rot_funcs[RATATE_FORMAT_NUM][4][3] = {
	[ROTATE_ARGB_8888] = {
		[0] = {
			(ROT_FUNC)rot_90_u32,
			(ROT_FUNC)rot_180_u32,
			(ROT_FUNC)rot_270_u32,
		},
	},
	[ROTATE_RGB_888] = {
		[0] = {
			NULL, NULL, NULL,
		},
	},
	[ROTATE_RGB_565] = {
		[0] = {
			(ROT_FUNC)rot_90_u16,
			(ROT_FUNC)rot_180_u16,
			(ROT_FUNC)rot_270_u16,
		},
	},
	[ROTATE_8BPP_MONO] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
	},
	[ROTATE_YUV420_SP] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[1] = {
			(ROT_FUNC)rot_90_u16,
			(ROT_FUNC)rot_180_u16,
			(ROT_FUNC)rot_270_u16,
		},
	},
	[ROTATE_YUV420_P] = {
		[0] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[1] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
		[2] = {
			(ROT_FUNC)rot_90_u8,
			(ROT_FUNC)rot_180_u8,
			(ROT_FUNC)rot_270_u8,
		},
	},
};

static int deg2index(int deg)
{
	switch (deg) {
	case ROTATE_DEG_90:
		return 0;
	case ROTATE_DEG_180:
		return 1;
	case ROTATE_DEG_270:
		return 2;
	default:
		return -1;
	}
}

/* return: 0(false) if invalid, otherwise !0(true) */
static unsigned char is_valid_addr(struct image *const im)
{
	unsigned int i = 0;
	unsigned long addr = 0;

	for (; i < im->channels; ++i) {
		addr |= (unsigned long)(im->datas[i].vir_addr);
	}
	return (0 != addr);
}

int neon_rotation_init(void)
{
	return 0;
}

int neon_rotation_quit(void)
{
	return 0;
}

int neon_rotate_sync(struct image *const src, struct image *dst, int deg)
{
	unsigned int channel;
	int deg_index = deg2index(deg);
	ROT_FUNC func[4];

	if ((RATATE_FORMAT_NUM <= src->pixel_format)
		|| (ROTATE_RGB_888 == src->pixel_format)
		|| !is_valid_addr(src)
		|| !is_valid_addr(dst)
		|| (-1 == deg_index)
		|| (src->width & 0x3)
		|| (src->height & 0x3)) {
		ALOGD("%s,%d: invalide paras !", __func__, __LINE__);
		return -1;
	}
	for (channel = 0; channel < src->channels; ++channel) {
		func[channel] = g_rot_funcs[src->pixel_format][channel][deg_index];
		if (NULL == func[channel])
			return -1;
	}
	for (channel = 0; channel < src->channels; ++channel) {
		char *src_addr = src->datas[channel].vir_addr
			+ src->datas[channel].crop.top * src->datas[channel].stride
			+ (src->datas[channel].crop.left * src->datas[channel].bpp >> 3);
		char *dst_addr = dst->datas[channel].vir_addr
			+ dst->datas[channel].crop.top * dst->datas[channel].stride
			+ (dst->datas[channel].crop.left * dst->datas[channel].bpp >> 3);
		(func[channel])(src_addr, src->datas[channel].stride,
			src->datas[channel].crop.right - src->datas[channel].crop.left,
			src->datas[channel].crop.bottom - src->datas[channel].crop.top,
			dst_addr, dst->datas[channel].stride);
	}
	return 0;
}

int neon_rotate_async(struct image *const src, struct image *dst, int deg)
{
	ALOGD("%s: no support yet.", __func__);
	return -1;
}

static int neon_rotation_destroy(struct rotation_module *module)
{
	free(module);
	neon_rotation_quit();
	return 0;
}

static struct rotation_module *neon_rotation_creat(void)
{
	struct rotation_module *module = NULL;

	if (neon_rotation_init())
		return NULL;

	module = (struct rotation_module *)malloc(sizeof(*module));
	if (NULL == module) {
		ALOGD("malloc for neon_rotation module fail.");
		neon_rotation_quit();
		return NULL;
	}

	memset((void *)module, 0, sizeof(*module));
	module->name = MODULE_NAME;
	module->destroy = neon_rotation_destroy;
	module->rotate_sync = neon_rotate_sync;
	module->rotate_async = neon_rotate_async;

	return module;
}

struct boot_strap neon_boot_strap = {
	MODULE_NAME,
	"neon rotation module",
	neon_rotation_creat,
};
