
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = demoVdecoder

demoVdecoder_SOURCES =  \
			demoVdecoder.c

demoVdecoder_CFLAGS = $(CFLAGS_CDXG)
demoVdecoder_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/libcore/stream/include \
        -I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/external/include/

demoVdecoder_CFLAGS += $(LOCAL_INCLUDE)
demoVdecoder_CPPFLAGS += $(LOCAL_INCLUDE)

demoVdecoder_LDFLAGS = $(LDFLAGS_CDXG)

demoVdecoder_LDADD = \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/parser/base/libcdx_parser.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/common/iniparser/libcdx_iniparser.la \
			$(top_srcdir)/libcore/common/plugin/libcdx_plugin.la

LOCAL_LIB = -lpthread -lz -lcdc_ve -lcdc_vdecoder -lcdc_memory

demoVdecoder_LDFLAGS += $(LOCAL_LIB)

