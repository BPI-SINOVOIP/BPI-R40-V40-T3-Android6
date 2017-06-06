
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = xplayerdemo

xplayerdemo_SOURCES =  ./xplayerdemo.cpp

xplayerdemo_CFLAGS = $(CFLAGS_CDXG)
xplayerdemo_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/xplayer  \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/stream/include \
		-I$(top_srcdir)/libcore/playback/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/../libcedarc/include

xplayerdemo_CFLAGS += $(LOCAL_INCLUDE)
xplayerdemo_CPPFLAGS += $(LOCAL_INCLUDE)

xplayerdemo_LDFLAGS = $(LDFLAGS_CDXG)

xplayerdemo_LDADD = $(top_srcdir)/libcore/parser/base/libcdx_parser.la \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/playback/libcdx_playback.la \
			$(top_srcdir)/xplayer/libxplayer.la \
			$(top_srcdir)/libcore/common/plugin/libcdx_plugin.la

LOCAL_LIB = -lpthread -lz -lcdc_vdecoder -lcdc_adecoder -lcdc_sdecoder -lcdc_ve \
		-lcrypto -lcdc_memory -lcdc_base -lnormal_audio
xplayerdemo_LDFLAGS += $(LOCAL_LIB)

