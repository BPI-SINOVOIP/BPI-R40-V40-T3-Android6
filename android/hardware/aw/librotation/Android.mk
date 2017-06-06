LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

librotation_source := rotation.c  cpu_rotate.c
rotate_cflags := -DLOG_TAG=\"rotation\"

ifeq ($(HAS_G2D_MODULE), true)
	librotation_source += g2d_rotate.c
	rotate_cflags += -DROTATION_G2D
endif

ifeq ($(HAS_TRANSFORM_MODULE), true)
	librotation_source += tr_rotate.c
	rotate_cflags += -DROTATION_TR
endif

ifeq ($(TARGET_ARCH_VARIANT), armv7-a-neon)
	librotation_source += neon_rotate.c \
		neon_do_rotate.S
	rotate_cflags += -DROTATION_NEON
endif

ifeq ($(ROTATION_ON_64BIT), true)
	rotate_cflags += -DROTATION_ON_64BIT
endif

# share library of Rotation
# ------------------------------------------
#$(warning $(librotation_source))
#$(warning $(rotate_cflags))
#$(warning $(LOCAL_MODULE_TARGET_ARCH))
#$(warning $(TARGET_ARCH_VARIANT))
LOCAL_SRC_FILES := $(librotation_source)
LOCAL_MODULE := librotation
LOCAL_SHARED_LIBRARIES := \
	libutils \
	libcutils \
	liblog
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(rotate_cflags)
include $(BUILD_SHARED_LIBRARY)

# static library of Rotation
# ------------------------------------------
include $(CLEAR_VARS)
#$(warning $(librotation_source))
#$(warning $(rotate_cflags))
#$(warning $(LOCAL_MODULE_TARGET_ARCH))
LOCAL_SRC_FILES := $(librotation_source)
LOCAL_MODULE := librotation_static
LOCAL_STATIC_LIBRARIES := \
	libutils \
	libcutils \
	liblog
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS:= $(rotate_cflags)
include $(BUILD_STATIC_LIBRARY)
