#include "g2d_rotate.h"
#include "g2d_driver.h"

#define MODULE_NAME "g2d"
#define DEVICE_NAME "/dev/g2d"

static int dev_fd = 0;

/*
* return (!0)(true) for success, return 0(flase) for fail.
*/
static unsigned char check_and_fix_input_para(
	struct image *const src, struct image *dst, int deg)
{
	unsigned int i = 0;

	if (!src || !dst) {
		ALOGD("%s,%d: src=%p, dst=%p", __func__, __LINE__, src, dst);
		return 0;
	}

	/* check the address */
	if (0 < src->channels) {
		for (i = 0; i < src->channels; ++i) {
			if (!(src->datas[i].phy_addr)) {
				ALOGD("phy_addr of datas[%p] is null !",
					(src->datas[i].phy_addr));
				return 0;
			}
		}
	} else {
		ALOGD("err channels %d!", src->channels);
		return 0;
	}
	return !0;
}

int g2d_init(void)
{
	dev_fd = open(DEVICE_NAME, O_RDWR);
	if (0 <= dev_fd) {
		return 0;
	} else {
		ALOGD("open %s fail, errno(%d).", DEVICE_NAME, errno);
		return -1;
	}
}

int g2d_quit(void)
{
	close(dev_fd);
	dev_fd = -1;
	return 0;
}

int g2d_rotate_sync(struct image *const src, struct image *dst, int deg)
{
	ALOGD("%s: no support yet.", __func__);
	return -1;
}

int g2d_rotate_async(struct image *const src, struct image *dst, int deg)
{
	ALOGD("%s: no support yet.", __func__);
	return -1;
}

static int g2d_rotation_destroy(struct rotation_module *module)
{
	free(module);
	g2d_quit();
	return 0;
}

static struct rotation_module *g2d_rotation_creat(void)
{
	struct rotation_module *module = NULL;

	if (g2d_init())
		return NULL;

	module = (struct rotation_module *)malloc(sizeof(*module));
	if (NULL == module) {
		ALOGD("malloc for g2d_rotation module fail.");
		g2d_quit();
		return NULL;
	}

	memset((void *)module, 0, sizeof(*module));
	module->name = MODULE_NAME;
	module->destroy = g2d_rotation_destroy;
	module->rotate_sync = g2d_rotate_sync;
	module->rotate_async = g2d_rotate_async;

	return module;
}

struct boot_strap g2d_boot_strap = {
	MODULE_NAME,
	"sunxi g2d module",
	g2d_rotation_creat,
};
