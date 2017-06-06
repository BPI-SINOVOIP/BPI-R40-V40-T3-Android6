
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libxplayer.la

libxplayer_la_SOURCES =  ./awMessageQueue.cpp \
			./awplayer.cpp \
			./cache.cpp \
			./demuxComponent.cpp

libxplayer_la_CFLAGS = $(CFLAGS_CDXG)
libxplayer_la_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/stream/include \
		-I$(top_srcdir)/libcore/playback/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/../libcedarc/include

libxplayer_la_CFLAGS += $(LOCAL_INCLUDE)
libxplayer_la_CPPFLAGS += $(LOCAL_INCLUDE)

libxplayer_la_LDFLAGS = $(LDFLAGS_CDXG)

libxplayer_la_LIBADD = $(top_srcdir)/libcore/parser/base/libcdx_parser.la \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/playback/libcdx_playback.la

LOCAL_LIB = -lpthread -lz -lcdc_vdecoder -lcdc_adecoder -lcdc_sdecoder
libxplayer_la_LDFLAGS += $(LOCAL_LIB)

