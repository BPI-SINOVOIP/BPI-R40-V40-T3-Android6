LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := watermark

LOCAL_MODULE_TAGS := option

LOCAL_SRC_FILES := \
    water_mark.c \
    main.c

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libui \
	libutils \

LOCAL_C_INCLUDES := \

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    water_mark.c \
	water_mark_interface.c

LOCAL_C_INCLUDES += \
	device/softwinner/common/hardware/camera/allwinnertech/include

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libnativehelper \
	libutils \
	libui \
	libdl

LOCAL_MODULE:= libwater_mark

include $(BUILD_SHARED_LIBRARY)

