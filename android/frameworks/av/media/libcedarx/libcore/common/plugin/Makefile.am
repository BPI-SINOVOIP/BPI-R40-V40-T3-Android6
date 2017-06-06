
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_plugin.la

libcdx_plugin_la_SOURCES =  ./cdx_plugin.c

libcdx_plugin_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../iniparser \
		-I../../include \
		-I../../base/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_plugin_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_plugin_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_plugin_la_LIBADD =  $(top_srcdir)/libcore/base/libcdx_base.la \
		$(top_srcdir)/libcore/common/iniparser/libcdx_iniparser.la

LOCAL_LIB = -lpthread -ldl -lcdc_vdecoder
libcdx_plugin_la_LDFLAGS += $(LOCAL_LIB)

