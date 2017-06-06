LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../config.mk

LOCAL_SRC_FILES = \
        $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 

LOCAL_C_INCLUDES:=  $(LOCAL_PATH)/../../../             \
                    $(LOCAL_PATH)/../../include         \
                    $(LOCAL_PATH)/../../base/include/   \
                    $(LOCAL_PATH)/../iniparser          \
                    $(LOCAL_PATH)/../                   \
                    $(TOP)/frameworks/av/media/libcedarc/include

LOCAL_CFLAGS += $(CDX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libcdx_plugin

LOCAL_SHARED_LIBRARIES:= libcutils         \
                         libdl             \
                         libcdx_base       \
                         libcdx_iniparser  \
                         libvideoengine

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_SHARED_LIBRARY)

