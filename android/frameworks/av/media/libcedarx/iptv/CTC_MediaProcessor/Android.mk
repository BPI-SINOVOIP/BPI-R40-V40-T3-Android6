LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


CEDARM_PATH=$(LOCAL_PATH)/../..
CEDARC_PATH=$(TOP)/frameworks/av/media/libcedarc

include $(CEDARM_PATH)/config.mk

LOCAL_SRC_FILES := \
    CTC_MediaProcessorImpl.cpp \
    IPTVcache.cpp \

IPTV_DEBUG := Y

ifeq ($(IPTV_DEBUG), Y)
LOCAL_C_INCLUDES  := \
    $(TOP)/frameworks/av/include/               \
	$(CEDARC_PATH)/include \
	$(CEDARC_PATH)/vdecoder/include \
	$(CEDARC_PATH)/adecoder/include \
	$(CEDARC_PATH)/sdecoder/include \
    $(CEDARM_PATH)/libcore/parser/include/ \
    $(CEDARM_PATH)/libcore/stream/include/ \
    $(CEDARM_PATH)/libcore/base/include/ \
	$(CEDARM_PATH)/libcore/playback/include \
	$(CEDARM_PATH)/libcore/include \
	$(LOCAL_PATH)/../demux/ \
	$(LOCAL_PATH)/../include/  \
	$(LOCAL_PATH)/../../   

else
LOCAL_C_INCLUDES  := \
    $(LOCAL_PATH)/../include/ 
endif        

LOCAL_C_INCLUDES  += $(LOCAL_PATH)/../../external/include/adecoder

LOCAL_SHARED_LIBRARIES := \
    libandroid_runtime \
    libcutils \
    libutils \
    libbinder \
    liblog \
    libui \
    libgui \
    libdl \
    libmedia \
    libplayer \
    libtvdemux \
    libcdx_parser       \
    libcdx_stream       \
    libcdx_base

LOCAL_MODULE := libCTC_MediaProcessor

LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS += -DMSTAR_MM_PLAYER=1 -g
LOCAL_CFLAGS += -DUSE_ANDROID_OVERLAY
LOCAL_CFLAGS += -DAndroid_4

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
