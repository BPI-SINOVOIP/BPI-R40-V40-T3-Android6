
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_playback.la

libcdx_playback_la_SOURCES =      audioDecComponent.cpp     \
    audioRenderComponent.cpp              \
    videoDecComponent.cpp                 \
    videoRenderComponent.cpp              \
    subtitleDecComponent.cpp              \
    subtitleRenderComponent.cpp           \
    avtimer.cpp                           \
    messageQueue.cpp                      \
    bitrateEstimater.cpp                  \
    framerateEstimater.cpp                \
    streamManager.cpp                     \
    player.cpp                            \
	deinterlace.cpp                   \
   soundControl/soundControl_null.cpp \
	layerControl/layerControl_null.cpp


libcdx_playback_la_CFLAGS = $(CFLAGS_CDXG)
libcdx_playback_la_CPPFLAGS = $(CFLAGS_CPPDXG)
LOCAL_INCLUDE = -I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include \
		-I./include \
		-I./subtitleNativeDisplay

libcdx_playback_la_CFLAGS += $(LOCAL_INCLUDE)
libcdx_playback_la_CPPFLAGS += $(LOCAL_INCLUDE)


libcdx_playback_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_playback_la_LIBADD = 

LOCAL_LIB = -lpthread -lrt -lcdc_vdecoder -lcdc_adecoder -lcdc_sdecoder -lcdc_memory
libcdx_playback_la_LDFLAGS += $(LOCAL_LIB)

