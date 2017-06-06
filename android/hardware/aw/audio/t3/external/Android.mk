# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := audio.external.$(TARGET_BOARD_PLATFORM)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SRC_FILES := audio_hw_external.c ../audio_iface.c ../audio_raw.c

# add for v40 bt call
ifneq ($(filter magton,$(TARGET_BOARD_PLATFORM)),)
LOCAL_SRC_FILES += audio_bt.c
endif

#ifneq ($(SW_BOARD_HAVE_3G), true)
#LOCAL_SRC_FILES += audio_ril_stub.c
#else
#LOCAL_SHARED_LIBRARIES := libaudio_ril
#endif

ifeq ($(KARAOK_PRODUCT), true)
LOCAL_CFLAGS += -DKARAOK_AUDIO_DEVICE
endif

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	system/media/audio_utils/include \
	system/media/audio_effects/include \
	system/media/audio_route/include \
	$(LOCAL_PATH)/../denoise/include
	
LOCAL_SHARED_LIBRARIES += liblog libcutils libtinyalsa libaudioutils libdl libcodec_audio libril_audio libaudioroute libaudio_utils

# add for V40
# flag MAGTON_AUDIO_DEVICE for audio_hw.c to choose V40 branch
# libAwDenoise for eliminating noise
ifneq ($(filter magton,$(TARGET_BOARD_PLATFORM)),)
LOCAL_CFLAGS += -DMAGTON_AUDIO_DEVICE
LOCAL_SHARED_LIBRARIES += libAwDenoise
endif

# add for V66
# flag ASTON_AUDIO_DEVICE for audio_hw.c to choose V66 branch
# libAwDenoise for eliminating noise
ifneq ($(filter aston,$(TARGET_BOARD_PLATFORM)),)
LOCAL_CFLAGS += -DASTON_AUDIO_DEVICE
LOCAL_SHARED_LIBRARIES += libAwDenoise
endif

# add for T3
# flag T3_AUDIO_DEVICE for audio_hw.c to choose t3 branch
# libAwDenoise for eliminating noise
ifneq ($(filter t3,$(TARGET_BOARD_PLATFORM)),)
LOCAL_CFLAGS += -DT3_AUDIO_DEVICE
LOCAL_SHARED_LIBRARIES += libAwDenoise
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under, $(LOCAL_PATH))
