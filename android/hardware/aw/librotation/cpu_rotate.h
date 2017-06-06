#ifndef __CPU_ROTATE_H__
#define __CPU_ROTATE_H__

#include "aw_rotation.h"
#include "rotation_sys.h"

int cpu_rotation_init(void);
int cpu_rotation_quit(void);
int cpu_rotate_sync(struct image *const src, struct image *dst, int deg);
int cpu_rotate_async(struct image *const src, struct image *dst, int deg);

#endif
