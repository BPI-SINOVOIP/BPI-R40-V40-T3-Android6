#ifndef __G2D_ROTATE_H__
#define __G2D_ROTATE_H__

#include "aw_rotation.h"
#include "rotation_sys.h"

int g2d_init(void);
int g2d_quit(void);
int g2d_rotate_sync(struct image *const src, struct image *dst, int deg);
int g2d_rotate_async(struct image *const src, struct image *dst, int deg);

#endif
