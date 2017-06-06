LOCAL_PATH:= $(call my-dir)

#
# libmediaplayerservice
#

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../libcedarx/config.mk

LOCAL_SRC_FILES:=               \
    ActivityManager.cpp         \
    Crypto.cpp                  \
    Drm.cpp                     \
    DrmSessionManager.cpp       \
    HDCP.cpp                    \
    MediaPlayerFactory.cpp      \
    MediaPlayerService.cpp      \
    MediaRecorderClient.cpp     \
    MetadataRetrieverClient.cpp \
    RemoteDisplay.cpp           \
    SharedLibrary.cpp           \
    StagefrightPlayer.cpp       \
    StagefrightRecorder.cpp     \
	AWStagefrightRecorder.cpp   \
    TestPlayerStub.cpp          \
    SimpleMediaFormatProbe.cpp	

LOCAL_SHARED_LIBRARIES :=       \
    libbinder                   \
    libcamera_client            \
    libcrypto                   \
    libcutils                   \
    libdrmframework             \
    liblog                      \
    libdl                       \
    libgui                      \
    libmedia                    \
    libmediautils               \
    libsonivox                  \
    libstagefright              \
    libstagefright_foundation   \
    libstagefright_httplive     \
    libstagefright_omx          \
    libstagefright_wfd          \
    libutils                    \
    libvorbisidec               \
    libawplayer             \
    libawmetadataretriever  \
    libthumbnailplayer          

LOCAL_STATIC_LIBRARIES :=       \
    libstagefright_nuplayer     \
    libstagefright_rtsp         \

LOCAL_C_INCLUDES :=                                                 \
    $(TOP)/frameworks/av/media/libstagefright/include               \
    $(TOP)/frameworks/av/media/libstagefright/rtsp                  \
    $(TOP)/frameworks/av/media/libstagefright/wifi-display          \
    $(TOP)/frameworks/av/media/libstagefright/webm                  \
    $(TOP)/frameworks/native/include/media/openmax                  \
    $(TOP)/external/tremolo/Tremolo                                 \
    $(TOP)/frameworks/av/media/libcedarx/                           \
    $(TOP)/frameworks/av/media/libcedarx/libcore/base/include/      \
    $(TOP)/frameworks/av/media/libcedarx/awplayer/                  \
    $(TOP)/frameworks/av/media/libcedarx/cmccplayer/                \
    $(TOP)/frameworks/av/media/libcedarx/metadataretriever/         \
    $(TOP)/frameworks/av/media/libcedarc/include                    \
    $(TOP)/frameworks/av/media/libcedarx/thumbnailplayer/   

#LOCAL_CFLAGS += -Werror -Wno-error=deprecated-declarations -Wall
LOCAL_CFLAGS += -Wall
LOCAL_CLANG := true

LOCAL_MODULE:= libmediaplayerservice

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
