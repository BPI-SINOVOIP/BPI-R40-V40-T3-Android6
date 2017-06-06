LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../config.mk

CEDARC = $(TOP)/frameworks/av/media/libcedarc/
CEDARX = $(LOCAL_PATH)/../../

################################################################################
## set flags for golobal compile and link setting.
################################################################################

CONFIG_FOR_COMPILE = 
CONFIG_FOR_LINK = 

LOCAL_CFLAGS += $(CONFIG_FOR_COMPILE)
LOCAL_MODULE_TAGS := optional

################################################################################
## set the source files
################################################################################
## set the source path to VPATH.
SourcePath = $(shell find $(LOCAL_PATH) -type d)
SvnPath = $(shell find $(LOCAL_PATH) -type d | grep ".svn")
SourcePath := $(filter-out $(SvnPath), $(SourcePath))


## set the source files.
tmpSourceFiles  = $(foreach dir,$(SourcePath),$(wildcard $(dir)/*.cpp))
SourceFiles  = $(foreach file,$(tmpSourceFiles),$(subst $(LOCAL_PATH)/,,$(file)))

## set the include path for compile flags.
LOCAL_SRC_FILES:= $(SourceFiles)
LOCAL_C_INCLUDES := $(SourcePath)                               \
                    $(LOCAL_PATH)/../../                        \
                    $(CEDARC)/ve/include                      \
                    $(CEDARC)/memory/include                  \
                    $(CEDARC)/include/                \
                    $(CEDARX)/libcore/base/                   \
                    $(CEDARX)/libcore/playback/include/        \
                    $(CEDARX)/libcore/parser/include      \
                    $(CEDARX)/libcore/stream/include    \
                    $(CEDARX)/libcore/base/include/     \
                    $(CEDARX)/libcore/include/     \
                    $(CEDARX)/external/include/adecoder/ \
                    $(CEDARX)/../../libcore/parser/include    \
                    $(CEDARX)/../../libcore/include  \
                    $(CEDARX)/../../libcore/playback/include  \
                    $(CEDARX)/../../external/include/adecoder 

LOCAL_SHARED_LIBRARIES := \
            libcutils       \
            libutils        \
            libplayer       \
            libvdecoder     \
            libadecoder     \
            libsdecoder     \
            libcdx_base     \
            libcdx_stream   \
            libcdx_parser   \
            libVE           \
            libMemAdapter

#libve
LOCAL_MODULE:= demoPlayer

include $(BUILD_EXECUTABLE)
