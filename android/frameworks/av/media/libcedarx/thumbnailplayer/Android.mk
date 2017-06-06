LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../config.mk

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
    	avtimer.cpp \
    	tplayer.cpp
		
LOCAL_C_INCLUDES  := \
        $(TOP)/frameworks/av/                               \
        $(TOP)/frameworks/av/include/                       \
        $(TOP)/frameworks/av/media/libcedarc/include \
        $(LOCAL_PATH)/../libcore/include/ \
        $(LOCAL_PATH)/..
        
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Werror -Wno-deprecated-declarations

LOCAL_MODULE:= libthumbnailplayer

LOCAL_SHARED_LIBRARIES +=   \
        libutils            \
        libcutils           \
        libbinder           \
        libmedia            \
        libui               \
        libgui              \
        libMemAdapter       \
        libvdecoder

include $(BUILD_SHARED_LIBRARY)

